//system includes
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define  RUN_TIME_PARSER 0
//netstorm includes
#include "url.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_log.h"
#include "logging.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "netstorm.h"
#include "ns_alloc.h" 
#include "ns_event_log.h"
#include "ns_string.h"
#include  "ns_script_parse.h"
#include "ns_sock_com.h"
#include "ns_msg_def.h"

//includes for this file only
#include "ns_socket_api_int.h"
#include "ns_connection_pool.h"

/* global variables */
char errorBuf[1024];
//turn on for debug output on stdout for testing
int debug =0;

//use this to set error for the session /page before we have even a cptr 
#define SET_PAGE_AND_SESSION_ERROR  { \
  VUser *vptr = TLS_GET_VPTR();\
  \
  vptr->sess_status = NS_REQUEST_ERRMISC; \
  vptr->page_status = NS_REQUEST_ERRMISC; \
}

//use this to close the connection when we only have an cptr, nothing else
#define CLOSE_CONNECTION  { \
  close(cptr->conn_fd);   \
  cptr->conn_fd = -1;   \
  FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);    \
  free_connection_slot(cptr, now);    \
}


/*macros*/

/*
 * translate error returned by system call 
 * x - message that is prefixed to the message we generate with strerror()
 * args - args with format as in printf()
 * errno is already known, so its not passed in
 */

#define LOG_STRERROR(x, args ...)   {\
  sprintf(errorBuf, "[%s, %s():%u] " "errno = %d (%s)\n" x, __FILE__, __FUNCTION__ ,__LINE__, errno, nslb_strerror(errno), ## args);  \
  fprintf(stderr, "%s\n",errorBuf);   \
} 

/*
 * translate error returned by getnameinfo, getaddrinfo. errno is passed into this macro
 * x - message that is prefixed to the message we generate
 * args - args with format as in printf()
 * y - errno to be translated
 */

#define LOG_GAI_STRERROR(x,y,args ...)   {\
  sprintf(errorBuf, "[%s, %s():%u] " "errno = %d (%s)\n" x, __FILE__, __FUNCTION__ ,__LINE__, (y), gai_strerror(y), ## args);  \
  fprintf(stderr, "%s\n",errorBuf);   \
} 


#define ERROR(x, args ...)  \
do {  \
  fprintf(stderr, "Error at [%s, %s():%u]" x, __FILE__, __FUNCTION__ ,__LINE__, ## args);  \
} while(0)

#define DEBUG(x, args ...)  \
do {  \
  if (debug)    \
  fprintf(stdout, "[%s, %s():%u]" x, __FILE__, __FUNCTION__ ,__LINE__, ## args);  \
} while(0)

//set y to 1 if read connection is closed
//
#define IS_READ_CONN_CLOSED(x,y) {  \
  char buf[1];    \
  int ret;        \
  y =0;       \
  ret = recv((x), buf, 1, MSG_PEEK);    \
  if ( (ret == 0) || ((ret < 0) && errno != EAGAIN))    { \
    NSDL2_API(NULL, NULL, "recv MSG_PEEK- ret = %d errno = %d\n",ret, errno);   \
    y =1;   \
  }       \
}

//xstr has to be a null terminated string -- like - 1234abcd
int 
htoi(u_char *xstr)
{
  int i, val =0;
  u_char *p;
  p = xstr;
  //if leading 0x present, skip
  if (xstr[0] == '0' && (xstr[1] == 'x' || xstr[1] == 'X')) {
    p += 2;
  }
  while (*p) {
    switch(*p) {
      case '0':case '1':case '2':case '3':case '4':
      case '5':case '6':case '7':case '8':case '9': 
        i = *p - '0';
        break;
      case 'a':case 'A':
        i = 10;
        break;
      case 'b':case 'B':
        i = 11;
        break;
      case 'c':case 'C':
        i = 12;
        break;
      case 'd':case 'D':
        i = 13;
        break;
      case 'e':case 'E':
        i = 14;
        break;
      case 'f':case 'F':
        i = 15;
        break;
      default: 
        ERROR("invalid byte %c\n",*p);
        return(-1);
    }
    val = (val << 4) | i;
    p++;
  }
  return(val);
}

int
unsetNonBlocking(int fd)
{
  int flags;
  if ( (flags = fcntl(fd, F_GETFL)) < 0) {
    LOG_STRERROR("fcntl F_GETFL");
    return(-1);
  }
  flags &= ~O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) {
    LOG_STRERROR("fcntl F_SETFL");
    return(-1);
  }
  return(0);
}

int
setNonBlocking(int fd)
{
  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
    LOG_STRERROR("fcntl");
    return(-1);
  }
  return(0);
}

int 
getNonBlocking(int fd)
{
  int flags;
  if ( (flags = fcntl(fd, F_GETFL)) < 0) {
    LOG_STRERROR("fcntl");
    return(-1);
  }
  if (flags & O_NONBLOCK) return(1);
  return(0);
}

int
setRecvTimeout(int fd, struct timeval *tv)
{
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)tv,  sizeof(struct timeval)) == -1) {
    LOG_STRERROR("setsockopt error");
    return(-1);
  }
  return(0);
} 

int
getRecvTimeout(int fd, struct timeval *tv)
{
  socklen_t len = sizeof(struct timeval);
  if (getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)tv, &len) == -1) {
    LOG_STRERROR("getsockopt error");
    return(-1);
  }
  return(0);
}

int
setSendTimeout(int fd, struct timeval *tv)
{
  if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)tv,  sizeof(struct timeval)) == -1) {
    LOG_STRERROR("setsockopt error");
    return(-1);
  }
  return(0);
} 

/*
 * select with timeout on a read socket
 * return
 * -1 - error
 *  0 - socket is ready for read
 *  1 - timeout
 *
 *  Inputs
 * fd - read or write fd as the case may be
 * mode - READ_SELECT/WRITE_SELECT
 *
 */

int
selectTimeout(int fd, int mode, struct timeval *tv )
{
  fd_set fdset;
  int ret, closed =0;

  NSDL2_API(NULL, NULL, "select timeout value: tv_sec %ld tv_usec %ld\n",tv->tv_sec, tv->tv_usec);
  while(1) {
    FD_ZERO(&fdset);
    FD_SET (fd, &fdset);
    if (mode == READ_SELECT)
      ret = select (fd+1, &fdset, NULL, NULL, tv);
    else if (mode == WRITE_SELECT)
      ret = select (fd+1, NULL, &fdset, NULL, tv);
    else {
      ERROR("error: invalid mode %d for select\n", mode);
      return(-1);
    }

    //0 is timeout, -1 is error,  > 0 is success
    if (ret <0) {
      if (errno == EAGAIN) {    //shouldn't get EAGAIN for blocking, but check anyway 
        if (getNonBlocking(fd))
          continue;
        else {// this is an error in case of blocking socket
          LOG_STRERROR("select error on fd %d\n",fd);
          return(-1);
        }
      }
      if (errno == EINTR) {    //comment out if we dont want to ignore EINTR
        NSDL2_API(NULL, NULL, "select got EINTR. continuing\n");
        continue;
      }
      LOG_STRERROR("select error");
      return(-1);

    } else if (ret >0) {
      if (!FD_ISSET(fd, &fdset)) {
        ERROR("error: select returned > 0 but fd is not ready");
        return(-1);
      }
      if (mode == READ_SELECT) {
        //select could also return > 0 if connection is closed
        IS_READ_CONN_CLOSED(fd, closed);
        if (closed) {
          ERROR("client closed connection on fd %d\n",fd);
          return(-1);
        }
      } else if (mode == WRITE_SELECT) {
        //we cant check if conn is closed with out writing 
      }
      return(0);
    } else if (ret == 0) { //timeout
      ERROR("error: select timeout\n");
      return(1);
    }
  } //while(1)
}

/* 
 * get token from <name> <separator> <value>..., return ptr to the value after separator
 *
 */
#define SEP '='
char* 
getNextToken (const char *nameval, int sep)
{
  char *p;
  if ( (p = strchr(nameval, sep)) == NULL) {
    ERROR("no separator found in %s\n",nameval);
    return(NULL);
  }
  p++;
  CLEAR_WHITE_SPACE(p);
  if (!*p) {
    ERROR("no value found in %s\n",nameval);
    return(NULL);
  }
  return(p);
}

/* convert addr to name */
int
addrToName(struct sockaddr *sap, socklen_t addrLen, char *host, int hostLen, char *service, int servLen)
{
  int ret;

  ret = getnameinfo(sap,
      addrLen, host, hostLen,
      service, servLen,     //NI_NUMERICHOST|NI_NUMERICSERV);
      NI_NAMEREQD);

  if (ret != 0) {
    LOG_GAI_STRERROR("getnameinfo",ret);
    return(-1);
  }
  return(0);
}

/*
 * extract hostname and port from x:y format
 * return
 * only hostname (no port)  if port found or no separator and port provided
 * error
 * NULL - missing port after separator, port out of range
 */
char*
extractHostname(char *name, char *portBuf)
{
  char *p;
  int temp;

  *portBuf =0;    //we check this on return to see if no port was supplied
  if (name == NULL) {
    return(NULL);
  }
  if ( (p = strchr(name, HOST_PORT_SEPARATOR)) != NULL) {
    p++;
    if (p == NULL) {
      ERROR("invalid or no port in hostname after separator %s\n",name);
      return(NULL);
    }
    temp = (u_short)atoi(p);
    if (temp > 65535) {
      ERROR("invalid port (>65535) \n");
      return(NULL);
    }
    //save port in string format for later use - _after_ validity check above
    strcpy(portBuf, p);
    //correct hostname by removing the port
    //localHostNameDup = strdup(localHostName);
    name[p-1-name] = 0;  //write 0 to index of separator
  } 
  return(name);
}


/*
 * Description: convert name to address
 * inputs
 * name - node name
 * return
 * addr - pointer to a linked list of addresses or
 * NULL on error
 *
 */

struct addrinfo *
nameToAddr (char *name, char* port)
{
  struct addrinfo hints;
  struct addrinfo *result;
  int ret;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM; /* tcp socket */
  hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
  hints.ai_protocol = 0;          /* Any protocol */
  hints.ai_canonname = NULL;    /* official name if AI_CANONNAME is set in ai_flags */
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  /* args to getaddrinfo
   * name - node name
   * NULL - service name
   * hints -specifications of the addresses to be returned
   * result - linked list of addresses (type addrinfo) 
   */
  ret = getaddrinfo(name, port, &hints, &result);
  if (ret != 0) {
    LOG_GAI_STRERROR("getaddrinfo",ret);
    return(NULL);
  }
  return(result);
}

/* 
 * Description: use select to timeout connect on a socket
 * Inputs: 
 * fd - socket to use for connect
 * addr - server address to connect to 
 * timeout - timeout 
 *
 * Outputs :
 *
 * Algo: 
 * Connect will return EINPROGRESS or EAGAIN if not immediate
 * Do a select with timeout until socket is ready for write, which
 * Indicates connect complete
 * Loop if select returns EINTR
 * 
 */

int 
connectTimeout (int fd, struct sockaddr *addr, int addrLen, struct timeval *tv)
{
  fd_set wr_set;
  socklen_t len;
  int val, err, ret;

  //use non blocking call
  //setNonBlocking(fd);
  NSDL2_API(NULL, NULL, "fd=%d  addr %s port %d \n",fd, inet_ntoa( ((struct sockaddr_in*)addr)->sin_addr), ntohs(((struct sockaddr_in*)addr)->sin_port) );

  err = connect (fd, (struct sockaddr*)addr, addrLen);
  if (err < 0) {
    if (errno == EINPROGRESS) {
      do {
        FD_ZERO(&wr_set);
        FD_SET (fd, &wr_set);
        ret = select (fd+1, NULL, &wr_set, NULL, tv);
        //0 is timeout, -1 is error but we break only if not EINTR, > 0 is success
        if ((ret <0) & (err != EINTR)) {
          LOG_STRERROR("select");
          return(-1);
        } else if (ret >0) {
          len = sizeof(val);
          if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &len) < 0)  {
            LOG_STRERROR("getsockopt");
            return(-1);
          }
          if (val != 0) {
            ERROR("got socket error %d (%s)\n",val, strerror(val));
            return(-1); 
          } else {
            break;
          }
        } else if (ret == 0) { //timeout
          LOG_STRERROR("select");
          return(-1);
        }
      } while (1);
    } else {
      LOG_STRERROR("connect"); //error other than EINPROGRESS
      return(-1);
    }
  } //connect returned < 0
  //unsetNonBlocking(fd);
  NSDL2_API(NULL, NULL, "connect to remote host succeeded \n");
  return(0);
}

/*
 * Description: Do an accept with timeout
 * Inputs: 
 * fd - old fd 
 * addr- address (placeholder to receive client address if needed or 
 * NULL 
 * tv - timeout value
 * 
 * Outputs: 
 * function returns new fd when accept is successful or error
 * Errors:
 * -1
 */

int 
acceptTimeout (int fd, void *addr, struct timeval *tv )
{
  fd_set rd_set;
  socklen_t acceptOptLen, reuseOptLen, addrLen ;
  int acceptOpt, reuseOpt, ret, new_fd=0;
  
  //check if the socket is a listening socket
  acceptOptLen = sizeof(acceptOpt);
  if ( (ret = getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptOpt, &acceptOptLen)) < 0) {
    LOG_STRERROR("getsockopt");
    return(-1);
  }
  if (acceptOpt != 1) {   //returns 1 for listening socket , 0 otherwise
    ERROR("fd = %d is not a listening socket\n",fd);
    return(-1); 
  }
  //set the socket to be reusable
  reuseOptLen = sizeof(reuseOpt);
  reuseOpt = 1;
  if (setsockopt(fd, SOL_SOCKET,  SO_REUSEADDR, (void*)&reuseOpt, reuseOptLen) < 0) {
    LOG_STRERROR("setsockopt error");
    return(-1);
  }

#if 0 //we're using timeout, so non blocking doesnt make sense
  if (setNonBlocking(fd)) {
    ERROR("setNonBlocking failed");
    return(-1);
  }
#endif

  while(1) {
    FD_ZERO(&rd_set);
    FD_SET (fd, &rd_set);
    ret = select (fd+1, &rd_set, NULL, NULL, tv);
    //0 is timeout, -1 is error,  > 0 is success
    if (ret <0) {
      if (errno == EAGAIN) {    //shouldn't get EAGAIN for blocking, but check anyway 
        if (getNonBlocking(fd))
          continue;
        else {// this is an error in case of blocking socket
          LOG_STRERROR("select error on fd %d\n",fd);
          return(-1);
        }
      }
      if (errno == EINTR) {    //comment out if we dont want to ignore EINTR
        NSDL2_API(NULL, NULL, "select got EINTR. continuing\n");
        continue;
      } 
      LOG_STRERROR("select error");
      return(-1);

    } else if (ret >0) {
      if (!FD_ISSET(fd, &rd_set)) {
        ERROR("error: select returned > 0 but fd is not ready");
        return(-1);
      }
     addrLen = sizeof(struct sockaddr);
      new_fd = accept (fd, (struct sockaddr*)addr, &addrLen);
      if (new_fd < 0) {
        LOG_STRERROR("accept error");
        return(-1);
      }
      break;
    } else if (ret == 0) { //timeout
      LOG_STRERROR(" error: select timed out");
      return(-1);
    }
  } //while(1)
#if 0 //we're using timeout, so non blocking doesnt make sense
  unsetNonBlocking(fd);
#endif
  NSDL2_API(NULL, NULL, "accept succeeded. new_fd = %d \n", new_fd);
  return(new_fd);
}

void 
userSockTimeoutHandle( ClientData client_data, u_ns_ts_t now) 
{
  connection* cptr;
  cptr = (connection *)client_data.p;
  VUser *vptr = cptr->vptr;

  NSDL4_API(NULL, cptr, "Method Called, cptr=%p conn state=%d", cptr, cptr->conn_state);

  cptr->conn_state = CNST_TIMEDOUT;
  //need to switch back to the user that had _this_ timeout
  switch_to_vuser_ctx(vptr, "timeout handler for user socket returns to user context");
}

void 
delete_user_socket_timeout_timer(connection *cptr) 
{
  NSDL4_API(NULL, cptr, "Method called, timer type = %d", cptr->timer_ptr->timer_type);

  if ( cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) {
    NSDL4_API(NULL, cptr, "Deleting Idle timer for user socket ");
    dis_timer_del(cptr->timer_ptr);
  }
}

/* 
 * Description:
 * set timeout timer for accept on this connection  
 */

  void
add_user_socket_accept_timer(connection *cptr)
{
  int timeout_val_sec, timeout_val_usec;  
  ClientData client_data;
  u_ns_ts_t now = get_ms_stamp();
  client_data.p = cptr; 
  VUser *vptr = cptr->vptr;
  timeout_val_sec = runprof_table_shr_mem[vptr->group_num].gset.userSockAcceptTimeout.tv_sec;
  timeout_val_usec = runprof_table_shr_mem[vptr->group_num].gset.userSockAcceptTimeout.tv_usec;
  if (timeout_val_sec == 0)
    timeout_val_sec = DEFAULT_ACCEPT_TIMEOUT;

  cptr->timer_ptr->actual_timeout = timeout_val_sec*1000 +timeout_val_usec/1000;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, userSockTimeoutHandle, client_data, 0 );

  NSDL2_API(NULL, NULL, "cptr %p accept timeout %d timer_ptr %p \n",cptr, cptr->timer_ptr->actual_timeout, cptr->timer_ptr); 
}



/* Description:
 * set a global value for accept timeout on a socket
 * NOTE: since there is no socket passed to this call, we can only set a value which applies 
 * to all sockets
 *
 * Inputs: 
 * seconds - timeout in seconds
 * u_sec- timeout in micro seconds
 * Outputs:
 * None
 * Algo: 
 */

void 
ns_sock_set_accept_timeout (long seconds, long u_sec)
{
  VUser *vptr = TLS_GET_VPTR();
  
  runprof_table_shr_mem[vptr->group_num].gset.userSockAcceptTimeout.tv_sec = seconds;
  runprof_table_shr_mem[vptr->group_num].gset.userSockAcceptTimeout.tv_usec = u_sec;
}

/* 
 * Description:
 * set timeout timer for connecton this connection  
 */

 void
add_user_socket_connect_timer(connection *cptr)
{
  int timeout_val_sec, timeout_val_usec;  
  ClientData client_data;
  u_ns_ts_t now = get_ms_stamp();
  VUser *vptr = cptr->vptr;
  client_data.p = cptr; 
  timeout_val_sec = runprof_table_shr_mem[vptr->group_num].gset.userSockConnectTimeout.tv_sec;
  timeout_val_usec = runprof_table_shr_mem[vptr->group_num].gset.userSockConnectTimeout.tv_usec;
  if (timeout_val_sec == 0)
    timeout_val_sec = DEFAULT_CONNECT_TIMEOUT;

  cptr->timer_ptr->actual_timeout = timeout_val_sec*1000 +timeout_val_usec/1000;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, userSockTimeoutHandle, client_data, 0 );

  NSDL2_API(NULL, NULL, "cptr %p connect timeout %d timer_ptr %p \n",cptr, cptr->timer_ptr->actual_timeout, cptr->timer_ptr); 

}

/* Description:
 * set a global value for connect timeout on a socket
 * NOTE: since there is no socket passed to this call, we can only set a value which applies 
 * to all sockets
 *
 * Inputs: 
 * seconds - timeout in seconds
 * u_sec- timeout in micro seconds
 * Outputs:
 * None
 * Algo: 
 */

void 
ns_sock_set_connect_timeout (long seconds, long u_sec)
{
  VUser *vptr = TLS_GET_VPTR();
  
  runprof_table_shr_mem[vptr->group_num].gset.userSockConnectTimeout.tv_sec = seconds;
  runprof_table_shr_mem[vptr->group_num].gset.userSockConnectTimeout.tv_usec = u_sec;
}

/* Description: set global recv timeout for recv’ing the expected
 * data on a socket
 * must be called before recv()
 * Inputs: 
 * seconds - timeout in seconds
 * u_sec- timeout in micro seconds
 * Outputs:
 * None
 * Algo: 
 */

  void 
ns_sock_set_recv_timeout (long seconds, long u_sec)
{
  VUser *vptr = TLS_GET_VPTR();
  
  runprof_table_shr_mem[vptr->group_num].gset.userSockRecvTimeout.tv_sec = seconds;
  runprof_table_shr_mem[vptr->group_num].gset.userSockRecvTimeout.tv_usec = u_sec;
}

/* 
 * Description:
 * set timeout timer for recv on this connection  
 */

  void
add_user_socket_recv_timer(connection *cptr)
{
  int timeout_val_sec, timeout_val_usec;  
  ClientData client_data;
  u_ns_ts_t now = get_ms_stamp();
  VUser *vptr = cptr->vptr;
  client_data.p = cptr; 
  
  timeout_val_sec = runprof_table_shr_mem[vptr->group_num].gset.userSockRecvTimeout.tv_sec;
  timeout_val_usec = runprof_table_shr_mem[vptr->group_num].gset.userSockRecvTimeout.tv_usec;
  if (timeout_val_sec == 0)
    timeout_val_sec = DEFAULT_RECV_TIMEOUT;

  cptr->timer_ptr->actual_timeout = timeout_val_sec*1000 +timeout_val_usec/1000;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, userSockTimeoutHandle, client_data, 0 );

  NSDL2_API(NULL, NULL, "cptr %p recv timeout %d timer_ptr %p \n",cptr, cptr->timer_ptr->actual_timeout, cptr->timer_ptr); 
}

/* Description: sets a timeout for receiving data on a socket
 * after connection is established
 * Inputs: 
 * seconds - timeout in seconds
 * u_sec- timeout in micro seconds
 * Outputs:
 * None
 * Algo: 
 */

void 
ns_sock_set_recv2_timeout (long seconds, long u_sec)
{
  VUser *vptr = TLS_GET_VPTR();
  
  runprof_table_shr_mem[vptr->group_num].gset.userSockRecv2Timeout.tv_sec = seconds;
  runprof_table_shr_mem[vptr->group_num].gset.userSockRecv2Timeout.tv_usec = u_sec;
}

/* 
 * Description:
 * set timeout timer for recv on this connection  
 */

  void
add_user_socket_recv2_timer(connection *cptr)
{
  int timeout_val_sec, timeout_val_usec;  
  ClientData client_data;
  u_ns_ts_t now = get_ms_stamp();
  VUser *vptr = cptr->vptr;
  client_data.p = cptr; 
  timeout_val_sec = runprof_table_shr_mem[vptr->group_num].gset.userSockRecv2Timeout.tv_sec;
  timeout_val_usec = runprof_table_shr_mem[vptr->group_num].gset.userSockRecv2Timeout.tv_usec;
  if (timeout_val_sec == 0)
    timeout_val_sec = DEFAULT_RECV2_TIMEOUT;

  cptr->timer_ptr->actual_timeout = timeout_val_sec*1000 +timeout_val_usec/1000;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, userSockTimeoutHandle, client_data, 0 );

  NSDL2_API(NULL, NULL, "cptr %p recv2 timeout %d timer_ptr %p \n",cptr, cptr->timer_ptr->actual_timeout, cptr->timer_ptr); 
}


/* 
 * Description:
 * set timeout timer for send on this connection  
 */

  void
add_user_socket_send_timer(connection *cptr)
{
  int timeout_val_sec, timeout_val_usec;  
  ClientData client_data;
  u_ns_ts_t now = get_ms_stamp();
  VUser *vptr = cptr->vptr;
  client_data.p = cptr; 
  timeout_val_sec = runprof_table_shr_mem[vptr->group_num].gset.userSockSendTimeout.tv_sec;
  timeout_val_usec = runprof_table_shr_mem[vptr->group_num].gset.userSockSendTimeout.tv_usec;
  if (timeout_val_sec == 0)
    timeout_val_sec = DEFAULT_RECV_TIMEOUT;

  cptr->timer_ptr->actual_timeout = timeout_val_sec*1000 +timeout_val_usec/1000;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, userSockTimeoutHandle, client_data, 0 );

  NSDL2_API(NULL, NULL, "cptr %p send timeout %d timer_ptr %p \n",cptr, cptr->timer_ptr->actual_timeout, cptr->timer_ptr); 
}


/* Description: sets a timeout for sending data on a socket
 * Inputs: 
 * seconds - timeout in seconds
 * u_sec- timeout in micro seconds
 * Outputs:
 * None
 * Algo: 
 */

void 
ns_sock_set_send_timeout (long seconds, long u_sec)
{
  VUser *vptr = TLS_GET_VPTR();
  
  runprof_table_shr_mem[vptr->group_num].gset.userSockSendTimeout.tv_sec = seconds;
  runprof_table_shr_mem[vptr->group_num].gset.userSockSendTimeout.tv_usec = u_sec;
}
/* In connection pool design, connections can be available in either or both
 * lists(reuse list, inuse list).
 * Here we are sending head node of each list, and performing connection cleanup
 * for cptr
 * Currently function is not used
 * */
static connection* 
fd_to_cptr(int fd)
{
  connection* cptr;
  
  VUser *vptr = TLS_GET_VPTR();
  
  
  if (vptr->head_cinuse)
  {
    for (cptr = vptr->head_cinuse; cptr != NULL; cptr = (connection *)cptr->next_inuse) {
      if (cptr->conn_fd == fd)  {
        return(cptr);
      }
    }
  }   
 
  // TODO - Check if we need to serach in reuse list or not
  if(vptr->head_creuse)
  { 
    for (cptr = vptr->head_creuse; cptr != NULL; cptr = (connection *)cptr->next_reuse) {
      if (cptr->conn_fd == fd)  {
        return(cptr);
      }
    }
  }
  
  NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "cptr for fd =%d not found",fd);
  return(NULL);
}

/* 
 * Description
 * close a socket 
 * Inputs
 *
 * char *s_desc,          socket descriptor 
 * Errors:
 * -1 
 */ 

int
ns_sock_close_socket(char *s_desc)
{
  int fd;
  connection* cptr;
  u_ns_ts_t now = get_ms_stamp();

  fd = ns_get_int_val (s_desc);
  if ( (cptr = fd_to_cptr(fd)) == NULL) { //no cptr - may be closed already
    return(0);
  }
  NSDL2_API(NULL, NULL, "closing cptr %p for descriptor fd =%d \n",cptr, fd);
  //usually the connection is already closed after send or recv. if close_fd is already done
  //its fd must be -1
  if (cptr->conn_fd != -1) {
    close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);  
  }
  return(0);
}
            
/* this is called from user context after epoll returns due to activity on the connect
 * socket 
 */
static int
check_user_socket_connect(connection* cptr, int events, u_ns_ts_t now)
{
  //ClientData client_data;
  //client_data.p = cptr;
  int val;
  socklen_t len;
  len = sizeof(val);
  
  NSDL2_API(NULL, cptr, "Method called, cptr=%p state=%d events=0x%x", cptr, cptr->conn_state, events);
  //before returning to user, check for epoll errors that indicate connect failure
  if (events & (EPOLLERR|EPOLLHUP)) {
    cptr->conn_state = CNST_CONNECT_FAIL; 
    //get socket error
    if (getsockopt(cptr->conn_fd, SOL_SOCKET, SO_ERROR, &val, &len) < 0)  {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"getsockopt failed: errno %d (%s)", errno, nslb_strerror(errno));
    }
    if (val != 0) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"getsockopt error: errno %d (%s)", errno, strerror(val));
    }
    return(-1);   //return -1 anyway
  } else if (!(events & EPOLLOUT)) {
    cptr->conn_state = CNST_CONNECT_FAIL; 
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"epoll events did nt return EPOLLOUT for connect");
    return(-1);
  } else {  // can add additional logic to double check connect here 
    cptr->conn_state = CNST_CONNECTED;
    NSDL2_API(NULL, NULL, "connect is OK\n");
  }

#if 0 // this calls retry_connection --> start_socket which we dont want
  if (double_check_connect(cptr, now) == -1) {
    return(-1);   //will return back to script
  } 
#endif
  return(0);
}


static int
start_user_socket_connect(connection* cptr)
{

  //ClientData client_data;
  //client_data.p = cptr;
  VUser *vptr = cptr->vptr;
  u_ns_ts_t now = get_ms_stamp();
  int ret;

  NSDL2_API(NULL, cptr, "Method called, cptr = %p, state = %d", cptr, cptr->conn_state);

  cptr->ns_component_start_time_stamp = now;//Update NS component start time
  //user_socket_avgtime->user_socket_num_con_initiated++; // Increment Number of TCP Connection initiated
  cptr->started_at = cptr->con_init_time = now;

#ifdef USE_EPOLL
  if(global_settings->high_perf_mode == 0) {
    num_set_select++; /*TST */
    NSDL2_API(NULL, cptr, "Going to add fd = %d in NVM's epoll", cptr->conn_fd);
    if (add_select(cptr, cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0) {
      ERROR("Error: Set select failed on user socket\n"); 
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "add select failed on user socket");
      CLOSE_CONNECTION
      SET_PAGE_AND_SESSION_ERROR  
      return(-1);
    }
  }
#endif
  //set timer for connect
    NSDL2_API(NULL, NULL, "connect timeout = %d cptr=%p timer_ptr=%p \n",cptr->timer_ptr->actual_timeout, cptr, cptr->timer_ptr); 
  add_user_socket_connect_timer(cptr);

  if (connect(cptr->conn_fd, (SA *) &(cptr->cur_server), sizeof(struct sockaddr_in6)) < 0 ) {
    if (errno == EINPROGRESS) {
      //TBD - if the user originally asked for non-blocking we must return here

      cptr->conn_state = CNST_CONNECTING;
      NSDL3_API(NULL, cptr, "setting conn_state for user socket cptr %p fd %d to CNST_CONNECTING",cptr, cptr->conn_fd);
#ifndef USE_EPOLL
      FD_SET( cptr->conn_fd, &g_wfdset );
#endif
      switch_to_nvm_ctx(vptr, "user socket connect to remote host gave EAGAIN");
      NSDL3_API(NULL, cptr,"vptr %p cptr %p fd %d switched back to user after epoll in NVM\n",vptr, cptr, cptr->conn_fd);
      NSDL2_API(NULL, NULL, "vptr %p cptr %p fd %d switched back to user after epoll in NVM\n",vptr, cptr, cptr->conn_fd);
      // if we switched at least once, we can only come back with a new state
      if (cptr->conn_state == CNST_TIMEDOUT) {
        Close_connection(cptr, 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"connect timed out for cptr %p fd %d\n", cptr, cptr->conn_fd);
        return(-1);
      } else {  // error or connect was successful, check each case
        //NOTE- (otherwise unused) header_state used to pass events from epoll
        ret = check_user_socket_connect(cptr, cptr->header_state, now);
        //Close_conn not done in check_user_connect. Errors logged.
        if (ret == -1) {
          Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
          return(-1); 
        }
      }
      assert(cptr->conn_state == CNST_CONNECTED);

    } else if (errno == ECONNREFUSED) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"Connection refused from server %s for user socket connect\n", nslb_get_src_addr(cptr->conn_fd));
      Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
      return(-1);
    }
  } else {    //connected right away
    cptr->conn_state = CNST_CONNECTED;
  }

  inc_con_num_and_succ(cptr); // Increment Number of TCP Connection success
  now = get_ms_stamp();//Need to calculate connect time
  cptr->connect_time = now - cptr->ns_component_start_time_stamp; //connection time diff
  cptr->ns_component_start_time_stamp = now;//Update NS component start time

  SET_MIN (average_time->url_conn_min_time, cptr->connect_time);
  SET_MAX (average_time->url_conn_max_time, cptr->connect_time);
  average_time->url_conn_tot_time += cptr->connect_time;
  average_time->url_conn_count++;

  NSDL2_API(NULL, NULL, "connect succeeded on cptr %p fd %d\n", cptr, cptr->conn_fd);
  /* 
   * we must now wait for the user to send data on this connection, so get back to the 
   * script.
   */

  return(0);
}

static int 
start_user_listen_socket(connection* cptr, int backlog)
{

  NSDL2_API(NULL, cptr, "Method called, cptr=%p state=%d", cptr, cptr->conn_state);
  u_ns_ts_t now = get_ms_stamp();

  // Socket for listen should not be marked as reuse
  if (cptr->conn_state == CNST_REUSE_CON) {
    NSDL2_API(NULL, NULL, "closing socket %d due to CNST_REUSE_CON \n",cptr->conn_fd);
    close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);  
  }

  if (cptr->num_retries == 0){ //This is the first try
    cptr->started_at = cptr->con_init_time = now;
  }

  if (add_select(cptr, cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0) {
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"set select failed on socket %d",cptr->conn_fd); 
    CLOSE_CONNECTION
    SET_PAGE_AND_SESSION_ERROR  
    return(-1);
  }
  if (listen (cptr->conn_fd, backlog) == -1) {
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"listen on socket %d failed. errno %d (%s)",cptr->conn_fd, errno, nslb_strerror(errno));
    Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
    return(-1);
  }
  cptr->conn_state = CNST_LISTENING;
  return(0);
}

static int ns_sock_create_socket_ex (char *s_desc, int type, char* localHostName, int localHostPort, char *remoteHostName, int remoteHostPort, int backlog)
{
  connection *cptr;
  int family = AF_INET;   //hardcoded for now
  int reuseOpt;
  int select_port;
  action_request_Shr *user_socket_url_num;
  userSocketData *user_socket_data;
  socklen_t reuseOptLen;
  u_ns_ts_t now = get_ms_stamp();
  VUser *vptr = TLS_GET_VPTR();
   
  NSDL2_API(NULL, NULL, "Method called: localHostName = %s, localHostPort = %d, backlog = %d, type = %d, "
                        "remoteHostName = %s, remoteHostPort = %d\n",
                         localHostName, localHostPort, backlog, type, remoteHostName, remoteHostPort);

  MY_MALLOC_AND_MEMSET(user_socket_url_num, sizeof(action_request_Shr), "user_socket_url_num", -1);
  MY_MALLOC_AND_MEMSET(user_socket_data, sizeof(userSocketData), "user_socket_data", -1);

  //memset(user_socket_url_num, 0, sizeof(action_request_Shr));
  //memset(user_socket_data, 0, sizeof(userSocketData));

  user_socket_url_num->request_type = USER_SOCKET_REQUEST;

  cptr = get_free_connection_slot(vptr);

  NSDL2_API(NULL, NULL, "cptr = %p, vptr = %p", cptr, vptr);
  if (cptr == NULL) {
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "free_connection not available");
    SET_PAGE_AND_SESSION_ERROR  
    return(-1);
  }

  cptr->data = (void*)user_socket_data;
  cptr->url_num = user_socket_url_num;
  //this is returned in get_request_type()
  cptr->request_type = user_socket_url_num->request_type;
  cptr->last_iov_base = NULL;
  memset(&(cptr->cur_server), 0, sizeof(struct sockaddr_in6));   //use for remote
  memset(&(cptr->sin), 0, sizeof(struct sockaddr_in6));    //use for local
  ((userSocketData*)cptr->data)->sockType = type;
  ((userSocketData*)cptr->data)->recvBuf = NULL;

  if (type == SOCK_STREAM) {
    cptr->conn_fd = get_socket(family, vptr->group_num);
  } else {    // For UDP socket, add to select here as we dont do much after this
    cptr->conn_fd = get_udp_socket(family, vptr->group_num);
#ifdef USE_EPOLL
    if(global_settings->high_perf_mode == 0) {
      num_set_select++; /*TST */
      if (add_select(cptr, cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0) {
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "add select failed on user UDP socket");
        goto err;
      }
    }
#endif
  }

  // save fd into user variable tables
  ns_set_int_val(s_desc, cptr->conn_fd); //save into descriptor string
  
  NSDL2_API(NULL, NULL, "s_desc = %s, conn_fd = %d", s_desc, cptr->conn_fd);
  if (setNonBlocking(cptr->conn_fd)) {
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "could not set fd %d to non blocking",cptr->conn_fd);
    goto err;
  }

  if (localHostName) { //TCP -bind to socket and listen, UDP - just bind
    //set the addr to be reusable
    reuseOptLen = sizeof(reuseOpt);
    reuseOpt = 1;
    /* this only applies to reusing an address that is bound to a socket which is now closed
     * and in the TIME_WAIT state. We cant bind twice to an address that is active 
     */
    if (setsockopt(cptr->conn_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuseOpt, reuseOptLen) < 0) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "setsockopt SO_REUSEADDR failed for socket %d errno %d (%s)",cptr->conn_fd, errno, nslb_strerror(errno));
      goto err;
    }

    if (!nslb_fill_sockaddr(&(cptr->sin), localHostName, localHostPort)) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "ns_fill_sockaddr failed");
      goto err;
    }
    if (localHostPort) {  //default is 0, so this means user provided a port
      if (bind (cptr->conn_fd, (struct sockaddr*)&(cptr->sin), sizeof(cptr->sin)) == -1) {
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "bind localhost (%s) for listen failed, errno %d (%s)",localHostName, errno, nslb_strerror(errno));
        goto err;
      }
    }
#if 1
    else {
      NSDL2_API(NULL, NULL, "my_port_index %d min_port %d max_port %d total_ip_entries %d vptr->user_ip->port %d\n",my_port_index, v_port_table[my_port_index].min_port,v_port_table[my_port_index].max_port, total_ip_entries, vptr->user_ip->port);
      struct sockaddr_in6 sin_temp;   //macro below will copy this back to cptr->sin
      memcpy ((char *)&(sin_temp), (char*)&(cptr->sin), sizeof(struct sockaddr_in6));
      /* macro below equires a special input of IP entries. it will not result in assigning 
       * a port otherwise. port # of 0 for a receiving TCP/UDP socket will result in the system 
       * assigning a random port.
       */
  
      BIND_SOCKET((char*)&(cptr->sin),
          v_port_table[my_port_index].min_port,
          v_port_table[my_port_index].max_port);

      NSDL2_API(NULL, NULL, "localHostPort that will be used %d \n", ntohs(cptr->sin.sin6_port));
    }
#endif

    //no listen for UDP, but we add to select and return here
    if (type == SOCK_DGRAM)  {    //UDP
      //nothing done here for now

    } else {    //TCP - this must be a listener 
      // errors and cleanup done inside the called routine
      if (start_user_listen_socket(cptr, backlog) == -1) {  //will add to select
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"failed to start listening socket");
        return(-1);
      }
      NSDL2_API(NULL, NULL, "listen successful for localhost = %s:%d sd = %d\n",localHostName,localHostPort, cptr->conn_fd);

    }
  } //localHostName

  if (remoteHostName) {   // TCP - connect to remote host
                          // UDP -get the remote host info to use later for recvfrom()

    if (type == SOCK_STREAM)  {   //TCP
      //bind to local port - not really needed 
      BIND_SOCKET((char*)&(vptr->user_ip->ip_addr), 
          v_port_table[my_port_index].min_port,
          v_port_table[my_port_index].max_port);

    }
#if 0
#define UDP_RECV_PORT 1111
      struct sockaddr_in si_me;
      si_me.sin_family = AF_INET;
      si_me.sin_port = htons(UDP_RECV_PORT);
      si_me.sin_addr.s_addr = htonl(INADDR_ANY);

      if (bind(cptr->conn_fd, &si_me, sizeof(si_me))==-1) {
        ERROR("bind failed for UDP socket\n"); 
        return(-1);
      }

    NSDL2_API(NULL, NULL, "Bind result- addr %s port %d min %d max %d \n",nslb_sock_ntop((struct sockaddr *)&(cptr->sin)), cptr->sin.sin6_port,v_port_table[my_port_index].min_port, v_port_table[my_port_index].max_port); 
#endif

    /*TCP and UDP both need the address of the remote server
     * TCP needs it to connect, UDP to send directly as it is connectionless
     */

    if (!nslb_fill_sockaddr(&(cptr->cur_server), remoteHostName, remoteHostPort)) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "ns_fill_sockaddr failed");
      goto err;
    }

    if (type == SOCK_DGRAM)  {    //UDP
      //no connect for UDP
    } else {  //TCP
      //For TCP this routine below adds the socket to select
      if (start_user_socket_connect(cptr) == -1) {  //errors and cleanup inside callee
        //user_socket_avgtime->user_socket_num_con_fail++;
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "connect failed");
        return(-1);
      }
    }
  } // remoteHostName 
  return(0);
err:
  CLOSE_CONNECTION
  SET_PAGE_AND_SESSION_ERROR  
  return(-1);
}

/* 
 * Description
 * create a socket (TCP/UDP) and bind to it. Do a listen or connect based on input
 *
 * Inputs
 *
 * char *s_desc,          socket descriptor 
 * char *type,             TCP/UDP
 * optional
 * [char* LocalHost,]    "LocalHost = <hostname/port #>"  hostname is optional here 
 * [char* peer,]          "RemoteHost = <hostname:port #>"
 * [char *backlog,]       "BackLog = <number>"
 * NSLastarg              marker to indicate end of parameters
 *
 * Algo:
 * 
 * Outputs:
 * 
 * Errors:
 * -1 
 */ 

#if RUN_TIME_PARSER
int 
ns_sock_create_socket (char *s_desc, char* type, ...)
  va_list ap;
  char *localHostName, *localHostNameDup, *remoteHostName, *remoteHostNameDup, *p;
#else
int ns_sock_create_socket (char *s_desc, char* type, char* localHostName, char *remoteHostName, char *backlogBuf)
#endif
{
  char *localHostPortBuf = DEFAULT_LOCAL_PORT_STR;
  char *remoteHostPortBuf= DEFAULT_REMOTE_PORT_STR; 
  u_short localHostPort = DEFAULT_LOCAL_PORT, remoteHostPort = DEFAULT_REMOTE_PORT;
  char tempPortBuf[8];
  u_char localHostSet =0, remoteHostSet =0;
  //u_char backlogSet = 0;
  int backlog = DEFAULT_BACKLOG, sockType;
  char *temp;

  NSDL2_API(NULL, NULL, "Method called. socket_descr = %s, type = %s\n", s_desc, type);

  //validate type // must be “TCP” or “UDP”
  if(!strncmp(type, "TCP", 3) )
    sockType = SOCK_STREAM;
  else if (!strncmp(type, "UDP", 3))
    sockType = SOCK_DGRAM;
  else { 
    NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "wrong value for type %s\n",type);
    SET_PAGE_AND_SESSION_ERROR  
    return(-1);
  }

#if RUN_TIME_PARSER
  va_start (ap, type);
  while (1) {
    name = va_arg(ap, char*);
    if (!strncmp(name, LASTARG_MARKER, strlen(LASTARG_MARKER))) {
      break;
    }
    count++;    //count the # of args - to avoid an infinite loop
    if (name == NULL || count > NS_CREATE_SOCKET_MAX_VARARGS) {
      ERROR("last arg marker %s not found. stop processing args\n",LASTARG_MARKER);
      return(-1);
    }
    if (!strncmp(name, "LocalHost", strlen("LocalHost"))) {
      //get hostname and port from host:port
      localHostSet = 1;
      if ( (localHostName = getNextToken(name,'=')) == NULL) {
        ERROR("no hostname found after LocalHost token\n");
        return(-1);
      }
      NSDL2_API(NULL, NULL, "localHostName before port extraction = %s\n",localHostName);
      temp = strdupa(localHostName);
      if ( (localHostName = extractHostname(temp, tempPortBuf)) == NULL) {
        return(-1);
      } 
      if (tempPortBuf[0] != 0) {
        localHostPortBuf = tempPortBuf;
        localHostPort = atoi(localHostPortBuf); 
      }
      NSDL2_API(NULL, NULL, "ret %d localHostName %s localHostPortBuf %s localHostPort %d\n",ret, localHostName, localHostPortBuf, localHostPort); 
    } else if (!strncmp(name, "RemoteHost", strlen("RemoteHost"))) {
      remoteHostSet = 1;
      if ( (remoteHostName = getNextToken(name,'=')) == NULL) {
        ERROR("no hostname found after RemoteHost token\n");
        return(-1);
      }
      NSDL2_API(NULL, NULL, "remoteHostName before port extraction = %s\n",remoteHostName);
      temp = strdupa(localHostName);
      if ( (remoteHostName = extractHostname(temp, tempPortBuf)) == NULL) {
        return(-1);
      } 
      if (tempPortBuf[0] != 0) {
        remoteHostPortBuf = tempPortBuf;
        remoteHostPort = atoi(remoteHostPortBuf); 
      }

      NSDL2_API(NULL, NULL, "remoteHostName %s remoteHostPortBuf %s remoteHostPort = %d\n",remoteHostName, remoteHostPortBuf, remoteHostPort); 
    } else if (!strncmp(name, "Backlog", strlen("Backlog"))) {
      //backlogSet = 1;
      //get backlog value
      if ( (p = getNextToken(name,'=')) == NULL) {
        ERROR("no value found after Backlog token\n");
        return(-1);
      }
      backlog = atoi(p);
      NSDL2_API(NULL, NULL, "backlog = %d\n",backlog); 
    } else {
      ERROR("illegal arg %s\n",name);
      return(-1);
    }
  } //while
  va_end(ap);
#else         // RUN_TIME_PARSER ends
  if (localHostName) {
    localHostSet =1;
    NSDL2_API(NULL, NULL, "localHostName before port extraction = %s\n",localHostName);
    temp = strdupa(localHostName);
    if ( (localHostName = extractHostname(temp, tempPortBuf)) == NULL) {
      NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "error extracting localHostName");
      SET_PAGE_AND_SESSION_ERROR  
      return(-1);
    }
    if (tempPortBuf[0] != 0) {
      localHostPortBuf = tempPortBuf;
      localHostPort = atoi(localHostPortBuf); 
    }
    NSDL2_API(NULL, NULL, "localHostName %s localHostPortBuf %s localHostPort %d\n",localHostName, localHostPortBuf, localHostPort); 
  }

  if (remoteHostName) {
    remoteHostSet =1;
    NSDL2_API(NULL, NULL, "remoteHostName before port extraction = %s\n",remoteHostName);
    temp = strdupa(remoteHostName);
    if ( (remoteHostName = extractHostname(temp, tempPortBuf)) == NULL) {
      NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "error extracting remoteHostName");
      SET_PAGE_AND_SESSION_ERROR  
      return(-1);
    } 
    if (tempPortBuf[0] != 0) {
      remoteHostPortBuf = tempPortBuf;
      remoteHostPort = atoi(remoteHostPortBuf); 
    }

    NSDL2_API(NULL, NULL, "remoteHostName %s remoteHostPortBuf %s remoteHostPort = %d\n",remoteHostName, remoteHostPortBuf, remoteHostPort); 
  }

  if (backlogBuf) {
    //backlogSet = 1;
    backlog = atoi(backlogBuf);
    NSDL2_API(NULL, NULL, "backlog = %d\n",backlog); 
  }
#endif    //!RUN_TIME_PARSER

  /* NOTES 
   *
   * for TCP, the socket either listens or connects
   * listen - needs only localHost to receive 
   * connect - needs only remoteHost to send
   * we cant have both, and cant have neither
   * for UDP
   * receiving - if remoteHost is provided (this must be provided during socket create, as 
   * the recv apis dont have a target address option), data is recvd from this host.
   * else, from anywhere like a TCP listener. localHost is required to bind socket.
   * sending - need remoteHost during socket creation (send can be called later w/o target 
   * here, data is sent to the remoteHost that was provided earlier)
   * if remoteHost is not provided at socket create time, send on this UDP 
   * socket will require a target in ns_sock_send(). 
   *
   * for localHost, the default address and any port may be used if not provided.
   */

  if (! (localHostSet || remoteHostSet)) { //neither is set. check applies to both TCP and UDP
    ERROR("neither LocalHost nor RemoteHost provided \n");
    NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "neither LocalHost nor RemoteHost provided");
    SET_PAGE_AND_SESSION_ERROR
    return(-1);
  }
  if (sockType == SOCK_STREAM) {
    if (localHostSet && remoteHostSet) { //both set for TCP 
      ERROR("Both LocalHost and RemoteHost provided \n");
      NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "LocalHost and RemoteHost provided- do we need listening or connecting socket ?");
      SET_PAGE_AND_SESSION_ERROR
      return(-1);
    }
  }
    

  if (ns_sock_create_socket_ex(s_desc, sockType, localHostName, localHostPort, remoteHostName, remoteHostPort, backlog) == -1) {
    return(-1);
  }

  return (0);
} //function ends

/* this is called from user context after epoll returns due to activity on the accept
 * socket 
 */

static int
check_user_socket_accept(connection* cptr, int events, u_ns_ts_t now)
{
  //ClientData client_data;
  //client_data.p = cptr;
  int val;
  socklen_t len;
  len = sizeof(val);
  
  NSDL2_API(NULL, cptr, "Method called, cptr=%p state=%d events=0x%x", cptr, cptr->conn_state, events);
  //before returning to user, check for epoll errors that indicate connect failure
  if (events & (EPOLLERR|EPOLLHUP)) {
    //get socket error
    if (getsockopt(cptr->conn_fd, SOL_SOCKET, SO_ERROR, &val, &len) < 0)  {
      LOG_STRERROR("getsockopt");
    }
    if (val != 0) {
      ERROR("got socket error %d (%s)\n",val, strerror(val));
    }
    return(-1);
  } else if (!(events & EPOLLIN)) {
    ERROR("epoll events did nt return EPOLLIN for accept. terminating accept\n");
    return(-1);
  } else {  // can add additional logic to double check connect here 
    cptr->conn_state = CNST_READY_TO_ACCEPT;
    cptr->connect_time = now - cptr->ns_component_start_time_stamp; //connection time diff
    cptr->ns_component_start_time_stamp = now;//Update NS component start time
    SET_MIN (average_time->url_conn_min_time, cptr->connect_time);
    SET_MAX (average_time->url_conn_max_time, cptr->connect_time);
    average_time->url_conn_tot_time += cptr->connect_time;
    average_time->url_conn_count++;

    NSDL2_API(NULL, NULL, "accept OK\n");
  }

#if 0 // this calls retry_connection --> start_socket which we dont want
  if (double_check_connect(cptr, now) == -1) {
    return(-1);   //will return back to script
  } 
#endif
  return(0);
}



int 
ns_sock_accept_connection_ex(connection *cptr) 
{
  int fd;
  u_ns_ts_t  now = get_ms_stamp();
  int count_switches =0;
  //ClientData client_data;
  int ret;
  VUser *vptr = cptr->vptr;

  //client_data.p = cptr;
  

  NSDL2_API(NULL, cptr, "ns_sock_accept_connection_ex entry. fd = %d\n", cptr->conn_fd);

  if (cptr->url_num->request_type != USER_SOCKET_REQUEST) {
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "Invalid request type %d", cptr->url_num->request_type);
    ERROR("Invalid request type %d\n", cptr->url_num->request_type);
    Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
    return(-2);
  }

  add_user_socket_accept_timer(cptr);

  while (1) {
    if ((fd = accept(cptr->conn_fd, NULL, 0)) < 0) {
      if (errno == EAGAIN) {
        //TBD - if the user originally asked for non-blocking we must return here

        if (cptr->conn_state == CNST_READY_TO_ACCEPT) { //doesnt happen 1st time in loop
          NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"accept failed even though state ==  CNST_READY_TO_ACCEPT fd %d count_switches %d conn_state 0x%x\n",cptr->conn_fd, count_switches, cptr->conn_state);
          Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
          return(-2);
        }

        count_switches++;
        NSDL2_API(NULL, NULL, "accept switching vptr %p cptr %p fd %d to user ctxt. count = %d\n",vptr, cptr, cptr->conn_fd, count_switches);
        switch_to_nvm_ctx(vptr, "user socket accept gave EAGAIN");
        NSDL2_API(NULL, NULL, "vptr %p cptr %p fd %d switched back to user after epoll in NVM\n",vptr, cptr, cptr->conn_fd);
        // if we switched at least once, we can only come back with a new state
        if (cptr->conn_state == CNST_TIMEDOUT) {
          Close_connection(cptr, 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
          NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"accept timed out for cptr %p fd %d\n", cptr, cptr->conn_fd);
          return(-2);
        } else {  // error or accept was successful, check each case
          //note- (otherwise unused) header_state used to pass events from epoll
          ret = check_user_socket_accept(cptr, cptr->header_state, now);
          if (ret == -1) {
            Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
            return(-2); 
          }
        }
        assert(cptr->conn_state == CNST_READY_TO_ACCEPT);
        continue;     //need to do accept() again to get new fd
      } else {
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"accept failed. fd %d count_switches %d conn_state 0x%x\n",cptr->conn_fd, count_switches, cptr->conn_state);
        Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
        return(-2); 
      }
    } else {
      cptr->conn_state =  CNST_ACCEPTED;
      cptr->connect_time = now - cptr->ns_component_start_time_stamp; //connection time diff
      cptr->ns_component_start_time_stamp = now;//Update NS component start time
      SET_MIN (average_time->url_conn_min_time, cptr->connect_time);
      SET_MAX (average_time->url_conn_max_time, cptr->connect_time);
      average_time->url_conn_tot_time += cptr->connect_time;
      average_time->url_conn_count++;
  
      NSDL2_API(NULL, NULL, "accept done.count_switches = %d state 0x%x\n",count_switches, cptr->conn_state);
      break;
    }
  } //while

  NSDL4_SOCKETS(NULL, cptr, "got fd from accept = %d", fd);

  /*Got new FD from accept. Close old FD */
  remove_select(cptr->conn_fd);
  if(close(cptr->conn_fd) < 0)
  {
    ERROR("Error in closing fd = %d\n", cptr->conn_fd);
  }

  cptr->conn_fd = fd;
  if (setNonBlocking(cptr->conn_fd)) {
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "setNonBlocking on new accept fd (%d) failed", cptr->conn_fd);
    //not calling Close_connection here because it tries to remove fd from select
    return(-1);
  }

  //add new desciptor to the select pool
  if (add_select(cptr, cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0) {
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"add select on new accept fd (%d) failed",cptr->conn_fd);
    return(-1);
  }
 

  inc_con_num_and_succ(cptr);
  // Check if setting now will cause any issues
  now = get_ms_stamp();//Need to calculate connect time
  cptr->connect_time = now - cptr->ns_component_start_time_stamp; //connection time diff
  cptr->ns_component_start_time_stamp = now;//Update NS component start time
  SET_MIN (average_time->url_conn_min_time, cptr->connect_time);
  SET_MAX (average_time->url_conn_max_time, cptr->connect_time);
  average_time->url_conn_tot_time += cptr->connect_time;
  average_time->url_conn_count++;
  NSDL2_API(NULL, NULL, "accept succeeded. new_fd = %d \n", fd);
  return(fd);
}


/* 
 * Description 
 * Accept connection on a socket and return a new socket
 *
 * Inputs 
 * oldSocket                old socket (listen)
 *
 * Outputs
 * newSocket                 new socket that accept returns
 *
 * Algo 
 * set timeout for accept (user provided or default) and do an accept() with select()
 */


int 
ns_sock_accept_connection (char *oldSocket, char *newSocket ) 
{
  int oldFd, newFd;
  connection *cptr;
  u_ns_ts_t  now = get_ms_stamp();

  oldFd = ns_get_int_val (oldSocket);
  if ( (cptr = fd_to_cptr(oldFd)) == NULL) {
    SET_PAGE_AND_SESSION_ERROR  
    return(-1);
  }
  newFd = ns_sock_accept_connection_ex(cptr);
  if (newFd < 0) {  //error
    //user_socket_avgtime->user_socket_num_con_fail++;
    if (newFd == -1) {  // didnt call Close_connection()
      CLOSE_CONNECTION
    }
    SET_PAGE_AND_SESSION_ERROR  
    return(-1);
  }
  ns_set_int_val (newSocket, newFd);
  return(0);
}

static void
ns_sock_free_vectors (connection *cptr)
{
  int j;
  NSDL2_API(NULL, NULL, "Method called, cptr %p", cptr);

  for (j = cptr->first_vector_offset; j < cptr->num_send_vectors; j++) {
    free_cptr_vector_idx(cptr, j);
    NSDL3_API(NULL, cptr, "num = %d size = %d", j, cptr->send_vector[j].iov_len);
  }
}
 
void debug_log_user_socket_data(connection *cptr, char *buf, int size, int complete_data, int first_trace_write_flag)
{
  VUser *vptr = TLS_GET_VPTR();
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        (request_type == USER_SOCKET_REQUEST &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_API))))
    return;

 {
    char log_file[1024];
    int log_fd;

    // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
  
    // put a ZERO in place of GET_PAGE_ID_BY_NAME(vptr) because we don't have page in socket  
    
    /* sprintf(log_file, "%s/logs/TR%d/user_socket_data_%d_%u_%u_%d_0_%d_%d_%d_0.dat", 
            g_ns_wdir, testidx, my_port_index, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr)); */
    
    sprintf(log_file, "%s/logs/TR%d/user_socket_data_%hd_%u_%u_%d_0_%d_%d_0_0.dat", 
            g_ns_wdir, testidx, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr)); 
    

    // Do not change the debug trace message as it is parsed by GUI
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
      NS_DT4(vptr, cptr, DM_L1, MM_API, "Response is in file '%s'", log_file);
    
    //Since response can come partialy so this will print debug trace many time
    //cptr->tcp_bytes_recv = 0, means this response comes first time

    if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging user socket data\n"
);
    else
    {
      write(log_fd, buf, size);
      close(log_fd);
    }
  }

  return;
}

#define SENDMSG 0     //tries to send everything in 1 send call. doesnt work

static int 
ns_user_socket_handle_write (connection *cptr, u_ns_ts_t now)
{
  int bytes_sent, bytes_to_send, bytes_left, offset, write_complete =0;
#ifdef NS_DEBUG_ON
  struct iovec *vector_ptr;
#endif
  struct iovec *start_vector;     /* Array of allocated vectors.*/
int i, write_loop_counter=0 ;
#if SENDMSG
  struct msghdr msg;
#endif
  VUser *vptr = cptr->vptr;

  NSDL2_API(NULL, cptr, "Method called cptr=%p conn state=%d,"
                        " proto_state = %d, size = %d, now = %u", 
                        cptr, cptr->conn_state, cptr->proto_state,
                        cptr->bytes_left_to_send, now);

  cptr->conn_state = CNST_WRITING;
  add_user_socket_send_timer(cptr); 

   while(!write_complete) {
      write_loop_counter++;
      start_vector = cptr->send_vector + cptr->first_vector_offset;
      bytes_to_send = cptr->bytes_left_to_send;

      NSDL2_API(NULL, cptr, "write loop count %d bytes_to_send %d",write_loop_counter, bytes_to_send);

      if ( ((userSocketData*)cptr->data)->sockType == SOCK_STREAM) {  //TCP
        bytes_sent = writev(cptr->conn_fd, start_vector, cptr->num_send_vectors - cptr->first_vector_offset);
        cptr->tcp_bytes_sent = bytes_sent;
        //user_socket_avgtime->user_socket_tx_bytes += bytes_sent;

      } else if ( ((userSocketData*)cptr->data)->sockType == SOCK_DGRAM) {  //UDP
#if SENDMSG
        memset(&msg, 0, sizeof(msg));
        msg.msg_name = &(cptr->cur_server);   //optional
        msg.msg_namelen = sizeof(struct sockaddr_in6);    //addr size
        msg.msg_iov = start_vector;
        msg.msg_iovlen = cptr->num_send_vectors - cptr->first_vector_offset;   // # elements in msg_iov
        // last arg = flags. TBD - this must be input by the api
        bytes_sent = sendmsg(cptr->conn_fd, &msg, 0);
        NSDL2_API(NULL, cptr, "UDP sendmsg done - num_vectors %d first_vector_offset %d bytes_sent %d",
                               cptr->num_send_vectors, cptr->first_vector_offset, bytes_sent );   
      
#else
        bytes_left = cptr->bytes_left_to_send;
        /* bytes_left, bytes_to_send and cptr->bytes_left_to_send are the same now. 
         * bytes_to_send  is modified below in the loop. and the bytes_left keeps track 
         * of how much is sent. 
         */ 


        offset =0;

        NSDL2_API(NULL, cptr, "starting UDP sendto - bytes_left %d bytes_to_send %d", bytes_left,bytes_to_send);

        while (bytes_left > 0) {
          bytes_to_send = MIN(bytes_left, MAX_UDP_SEND_SIZE);
          bytes_sent  = sendto (cptr->conn_fd, (void*) vector->iov_base+offset, bytes_to_send, 0, &(cptr->cur_server), sizeof(struct sockaddr_in6) );
          NSDL2_API(NULL, NULL, "UDP sendto loop - bytes_left %d bytes_to_send %d bytes_sent %d\n", bytes_left,bytes_to_send, bytes_sent);
          if (bytes_sent < 0) break;
          bytes_left -= bytes_sent;
          cptr->bytes_left_to_send = bytes_left;
          offset += bytes_sent;
        }   //while
#endif
      } else {
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "unknown protocol type");
        Close_connection(cptr, 0, now, NS_REQUEST_ERRMISC, NS_COMPLETION_NOT_DONE);
        return(-1);
      }

      /* NOTE:
       *  calculate_child_epoll_timeout is called only in the main child loop.this is where we
       *  check for timeouts. Hence when we are in this write loop (current routine), we either 
       *  need to call the timeout routine as we loop, or just check when we switch to the NVM and
       *  return, assuming that we do switch at some time. currently, we use the latter 
       *  (approximate) approach
       */

      if (bytes_sent < 0)  {
        if (errno == EAGAIN) {
          //TBD - if the user originally asked for non-blocking we must return here
          NSDL2_API(NULL, cptr, "user socket: vptr %p cptr %p fd %d EAGAIN for write",vptr, cptr, cptr->conn_fd);
          switch_to_nvm_ctx(vptr, "user socket: EAGAIN for write");
          if (cptr->conn_state == CNST_TIMEDOUT) {
            Close_connection(cptr, 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
            NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "write timed out (case 1) for cptr %p fd %d\n", cptr, cptr->conn_fd);
            NSDL2_API(NULL, cptr, "write timed out (case 1) for cptr %p fd %d\n", cptr, cptr->conn_fd);
            return(-1);
          }
          NSDL2_API(NULL, cptr, "vptr %p cptr %p fd %d write switched back to user after epoll in NVM",vptr, cptr, cptr->conn_fd);
          continue;
        } else {  //bad error, return
          ns_sock_free_vectors(cptr);
          NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,
              "Unable to send complete msg on user socket,"
              "bytes_sent = %d, bytes_to_send= %d",
              bytes_sent, bytes_to_send);
          Close_connection(cptr, 0, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE);
          return(-1);
        }
      }

      //UDP should nt come here as it doesnt do partial data 
      if (((userSocketData*)cptr->data)->sockType == SOCK_DGRAM) break;
         

      /*
       * we wrote partial data this time and need to readjust things and do another write
       * the rearrangement of vectors is needed because we can only pass complete vectors to
       * the writev routine (essentially rearranging means, we point the base of the first 
       * vector that we send to writev to the 
       * offset of the last incomplete vector and adjust its size, and shift this and everything
       * else to the front). 
       */
      if (bytes_sent < bytes_to_send) {
        NSIOVector ns_iovec;
        ns_iovec.vector = start_vector;
        ns_iovec.cur_idx = cptr->num_send_vectors;
        for(i = 0; i < ns_iovec.cur_idx; i++)
        {
          if(cptr->free_array[i])
            ns_iovec.flags[i] |= NS_IOVEC_FREE_FLAG; 
        }
        handle_incomplete_write(cptr, &ns_iovec, ns_iovec.cur_idx, bytes_to_send, bytes_sent);
        //no need to go back to epoll as the socket may be considered ready until EAGAIN
        continue;
      }
      assert(bytes_sent == bytes_to_send);
      NSDL2_API(NULL, cptr, "write completed ok. bytes_to_send %d bytes_sent %d",bytes_to_send,bytes_sent);
      write_complete =1;
    } //while

#ifdef NS_DEBUG_ON
  /*
   * log whatever was sent after the last incomplete write
   * handle_incomplete_write logs data for the incomplete writes
   */
  for (i = cptr->first_vector_offset, vector_ptr = start_vector; i < cptr->num_send_vectors; i++, vector_ptr++)
  {
    debug_log_user_socket_data(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
  }
#endif
 
  cptr->bytes_left_to_send = 0;
  //set the next state if needed
  return(0);
}

#define TCP 1     //will work for UDP send with 1 vector as well
#define MULTI_SENDMSG 0   //not used 
/*
 * make a request and send it 
 */
int ns_sock_send_ex(connection *cptr, char *data, int len, int flags) 
{
  struct iovec vector[IOVECTOR_SIZE];
  char free_array[IOVECTOR_SIZE];
  int next_idx = 0;
  u_ns_ts_t now = get_ms_stamp();
  int ret;
#if MULTI_SENDMSG
  int i=0, n, nleft;
#endif

  NSDL2_API(NULL, cptr, "Method called cptr %p conn state 0x%x data %p len %d flags 0x%x",
                cptr, cptr->conn_state, data, len, flags);

  next_idx = 0;

#if TCP
  vector[next_idx].iov_base = (void*)data;
  vector[next_idx].iov_len = len;
  free_array[next_idx] = 1;
  next_idx++;
  cptr->send_vector = vector;
  cptr->bytes_left_to_send = len;
  cptr->num_send_vectors = next_idx;
  cptr->first_vector_offset = 0;
  cptr->free_array = free_array; 
#elif MULTI_SENDMSG
  n = len/MAX_UDP_SEND_SIZE;
  nleft = len % MAX_UDP_SEND_SIZE;

  for (i=0; i<n; i++) {
    vector[i].iov_base = (void*)data+ i*MAX_UDP_SEND_SIZE;
    vector[i].iov_len = MAX_UDP_SEND_SIZE;
    free_array[i] = 1;
  }
  next_idx = i;

  if (nleft) {
    vector[i].iov_base = (void*)data+ i*MAX_UDP_SEND_SIZE;
    vector[i].iov_len = nleft;
    free_array[i] = 1;
    next_idx++;
  }

  cptr->send_vector = vector;
  cptr->bytes_left_to_send = len;
  cptr->num_send_vectors = next_idx;
  cptr->first_vector_offset = 0;
  cptr->free_array = free_array; 
#endif
  ret = ns_user_socket_handle_write(cptr, now);
  if (ret == -1)    //Close_conn already called for these in handle_write(). so return
    return(-1);
  now = get_ms_stamp();
  
  //if there are no errors, leave connection open as user may use it again
  //Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);

  return(ret);
}

/* 
 * Description: 
 * Send data on connected socket (TCP) or UDP socket.  
 *
 * Inputs: 
 * char *socket_desc         socket descriptor (name)
 * char *bufName             name of the buffer that contains data
 * optional 
 * target host in the format "TargetSocket=host:port" 
 * flags -                   "Flags=<flags>" contains flags to pass into the send() call
 *
 * Outputs: None
 * returns:
 * no of bytes sent or -1 on error
 *
 * Algo 
 *
 */


#if RUN_TIME_PARSER
int 
ns_sock_send (char *socket_desc, char* bufName,...) 
  va_list ap;
  char *targetHostName, *p = NULL, *dataVal, name;  
  int count;
#else
int 
ns_sock_send (char *socket_desc, char* bufName, char *targetHostName, char* flags) 
#endif
{
  int fd, ret;
  long dataLen;
  IW_UNUSED(int targetHostPort = DEFAULT_TARGET_PORT);
  //u_char targetHostSet =0;
  //u_char sendFlagsSet =0; 
  char *p = NULL, *dataVal;  
  char tempPortBuf[8];
  IW_UNUSED(char *targetHostPortBuf = DEFAULT_TARGET_PORT_STR);
  //struct timeval tv;
  int sendFlags =0;
  //int nSend;
  //struct addrinfo *haddr;
  char temp[128], *tmp;
  connection *cptr;
  u_ns_ts_t now = get_ms_stamp();

  NSDL2_API(NULL, NULL, "Method called, socket_desc = %s, bufName = %s", socket_desc, bufName);
#if RUN_TIME_PARSER
  //extract optional args
  va_start (ap, bufName);
  while (1) {
    name = va_arg(ap, char*);
    if (name == NULL) {
      ERROR("last arg marker %s not found. stop processing args\n",LASTARG_MARKER);
      return(-1);
    }
    if (!strncmp(name, LASTARG_MARKER, strlen(LASTARG_MARKER))) {
      break;
    }
    count++;    //count the # of args - to avoid an infinite loop
    if (count > NS_SEND_MAX_VARARGS) {
      ERROR("last arg marker %s not found. stop processing args\n",LASTARG_MARKER);
      return(-1);
    }
    //check that dest_ip is given for UDP
	
   if (!strncmp(name, "TargetSocket", strlen("TargetSocket"))) {
      //get hostname and port from host/port
      //targetHostSet = 1;
      if ( (targetHostName = getNextToken(name,'=')) == NULL) {
        ERROR("no hostname found after TargetSocket token\n");
        return(-1);
      }
      tmp = strdupa(targetHostName);
      if ( (targetHostName = extractHostname(tmp, tempPortBuf)) == -1) {
        return(-1);
      } 
      if (tempPortBuf[0] != 0) {
        IW_UNUSED(targetHostPortBuf = tempPortBuf);
        IW_UNUSED(targetHostPort = atoi(targetHostPortBuf)); 
      }
      NSDL2_API(NULL, NULL, "targetHostName %s targetHostPortBuf %s targetHostPort %d\n",targetHostName, targetHostPortBuf, targetHostPort); 
   } else if (!strncmp(name, "Flags", strlen("Flags"))) {
       //sendFlagsSet = 1;
     //get flags for send
     if ( (p = getNextToken(name,'=')) == NULL) {
       ERROR("no value found after Flags token\n");
       return(-1);
     }
     sendFlags = atoi(p);
     NSDL2_API(NULL, NULL, "sendFlags = 0x%x\n",sendFlags); 
   } else {
     ERROR("illegal arg %s\n",name);
     return(-1);
   }
  }   //while(1)
  va_end(ap);  
#else         // RUN_TIME_PARSER ends
  if (targetHostName) {
      //targetHostSet = 1;
      NSDL2_API(NULL, NULL, "targetHostName before port extraction = %s\n",targetHostName);
    tmp = strdupa(targetHostName); 
    if ( (targetHostName = extractHostname(tmp, tempPortBuf)) == NULL) {
      NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "error extracting targetHostName");
      goto err;
    }
    if (tempPortBuf[0] != 0) {
      IW_UNUSED(targetHostPortBuf = tempPortBuf);
      IW_UNUSED(targetHostPort = atoi(targetHostPortBuf)); 
    }
    NSDL2_API(NULL, NULL, "targetHostName %s targetHostPortBuf %s targetHostPort %d\n",targetHostName, targetHostPortBuf, targetHostPort); 
  }

  if (flags) {
     //sendFlagsSet = 1;
     sendFlags = atoi(p);
     NSDL2_API(NULL, NULL, "sendFlags = 0x%x\n",sendFlags); 
  }
#endif    //!RUN_TIME_PARSER
  //check that we have a connection
  fd = ns_get_int_val(socket_desc);
  if ( (cptr = fd_to_cptr(fd)) == NULL) {
    SET_PAGE_AND_SESSION_ERROR  
    return(-1); 
  }

  if ( (((userSocketData*)cptr->data)->sockType == SOCK_STREAM) &&
      (cptr->conn_state != CNST_CONNECTED) && 
      (cptr->conn_state != CNST_ACCEPTED) &&  // we may accept conn on listener and then send
      (cptr->conn_state != CNST_WRITING)) {     //multiple ns_sock_send s can happen
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"TCP socket %d not in right state 0x%x\n", fd, cptr->conn_state);
    goto err;
  }

  // get binary data (or string) saved by user in variable named -bufName
  memset(temp, 0, sizeof(temp));
  sprintf(temp, "{%s}",bufName);
	if ( (p = ns_eval_string_flag (temp, 0, &dataLen)) == NULL) {
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"ns_eval_string_flag  returned NULL for %s\n",bufName);
    goto err;
  }
  
   NSDL2_API(NULL, NULL, "Sending message: p = %s, dataLen = %d", p, dataLen); 
  //make a copy of the data and send it
  MALLOC_AND_MEMCPY(p, dataVal, dataLen, "send data buffer", -1);
   
  ret = ns_sock_send_ex(cptr, dataVal, dataLen, sendFlags); 
  /* ns_sock_send_ex and the routines it calls, call Close_connection() for errors, so we can 
   * just return here
   */
  NSDL2_API(NULL, NULL, "ret = %d", ret);
  return(ret);

err:
  NSDL2_API(NULL, NULL, "Method exit");
  now = get_ms_stamp();
  Close_connection(cptr, 0, now, NS_REQUEST_ERRMISC, NS_COMPLETION_NOT_DONE);
  return(-1);
}

int
ns_user_socket_handle_read(connection *cptr, int size, char *terminator, char *mismatch, int timeoutFlag, int flags)
{
  int recvBufSize, bytesRecvd, bytesToRecv, bytesLeft; 
  int request_type;
  int offset = 0, iTerm =0 ,count =0, matchFound =0, binaryTerm =0, byteVal ;
  struct sockaddr *paddr = NULL;
  socklen_t paddrLen;
  char *recvBuf = NULL;
  u_char byteBuf[3];
  VUser *vptr = cptr->vptr;

  u_ns_ts_t  now = get_ms_stamp();

  NSDL2_API(NULL, cptr, "ns_user_socket_handle_read entry: cptr %p fd %d size %d terminator %s mismatch %s flags 0x%x conn state=%d, now = %u recvBuf %p", cptr, cptr->conn_fd, size, (terminator)?terminator:"NULL", (mismatch)?mismatch:"NULL", flags, cptr->conn_state, now, recvBuf);

  request_type = cptr->url_num->request_type;
  if (request_type != USER_SOCKET_REQUEST) { 
    /* Something is very wrong we should not be here. */
    fprintf(stderr, "Request type is not user socket but we're handling the read\n");
    return(-1);
  }
  cptr->conn_state = CNST_READING;
  add_user_socket_recv_timer(cptr); 

  if ( ((userSocketData*)cptr->data)->sockType == SOCK_DGRAM) {  //UDP
    paddr = (struct sockaddr*)&(cptr->cur_server);   
    paddrLen = sizeof(struct sockaddr_in6);    //addr size
    NSDL2_API(NULL, cptr, "socket type is UDP. server %s",nslb_sock_ntop((struct sockaddr *)&(cptr->cur_server)));   
  } else 
    NSDL2_API(NULL, cptr, "socket type is TCP");

  if (terminator) {
    if ( (terminator[0] == '\\') &&  ((terminator[1] == 'x') || (terminator[1] == 'X')) ) {
      binaryTerm = 1;
    }
  }

  bytesLeft = size;
  offset = 0;       //offset into the recv buffer = total bytes recvd so far
  count =0;   //counter - incremented when we alloc or realloc

 
  recvBufSize =0;

  while (1) {
    /* we allocate in increments of NS_RECV_BUFSIZE 
     * for the case where there is no terminator, the recv is is done in NS_RECV_BUFSIZE 
     * increments. If we have a terminator, we recv 1 byte at a time
     * we always allocate in chunks of NS_RECV_BUFSIZE however
     *
     * recvBufSize-offset = unconsumed part of the recv buffer
     * what is the check here ?
     * remaining space in the receiving buffer must be atleast NS_RECV_BUFSIZE as we intend
     * to recv this much, unless the total # of bytes remaining is itself <=
     * NS_RECV_BUFSIZE - in which case we dont need more space
     */
    if ((recvBufSize-offset) < MIN(NS_RECV_BUFSIZE, bytesLeft)) {
      //have to increment first as realloc needs the new size
      recvBufSize += NS_RECV_BUFSIZE;
      MY_REALLOC(recvBuf, recvBufSize, "user socket recv buffer", count);
      /* we normally assign this after data is recvd. however, its possible that we will 
       * realloc to recv more and then exit the loop w/o getting anything. so assign here so
       * that we have the latest buffer address, if the address changed
       */
      if (((userSocketData*)cptr->data)->recvBuf != recvBuf)
      {
        ((userSocketData*)cptr->data)->recvBuf = recvBuf;
        ((userSocketData*)cptr->data)->recvBufBytes = 0;
      }
      count++; 
    }
    //terminator is a string of bytes that indicates end marker of the message to be recvd
    if (terminator || mismatch)
      bytesToRecv = 1; 
    else
      bytesToRecv = MIN(bytesLeft, NS_RECV_BUFSIZE);

    bytesRecvd = recvfrom (cptr->conn_fd, recvBuf+offset, bytesToRecv, flags, paddr, &paddrLen);

    NSDL2_API(NULL, cptr, "bytesRecvd = %d",bytesRecvd); 

    if (bytesRecvd < 0) {
      if (errno == EAGAIN) {
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_rfdset );
#endif
        //TBD - if the user originally asked for non-blocking we must return here

        NSDL2_API(NULL, cptr, "user socket: vptr %p cptr %p fd %d EAGAIN for read",vptr, cptr, cptr->conn_fd);
        switch_to_nvm_ctx(vptr, "user socket: EAGAIN for read");
        if (cptr->conn_state == CNST_TIMEDOUT) {
          // if we were asked to recv until TO, return normally
          if (timeoutFlag && (((userSocketData*)cptr->data)->recvBufBytes != 0) ) {
            NSDL2_API(NULL, cptr, "read timed out, but returning normally as we were supposed to recv until TO");
            break;
          }
          NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "read timed out (case 1) for cptr %p fd %d\n", cptr, cptr->conn_fd);
          NSDL2_API(NULL, cptr, "read timed out (case 1) for cptr %p fd %d\n", cptr, cptr->conn_fd);
          Close_connection(cptr, 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
          return(-1);
        }
        NSDL2_API(NULL, NULL, "user socket: vptr %p cptr %p fd %d read switched back to user after epoll in NVM\n",vptr, cptr, cptr->conn_fd);
        continue;   //get back to read in the loop
      } else {
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,
        "user socket: read failed (%s) for main: host = %s", nslb_strerror(errno), nslb_sock_ntop((struct sockaddr *)&(cptr->cur_server)));
        Close_connection(cptr, 0, now, NS_REQUEST_NO_READ, NS_COMPLETION_BAD_READ);
        return(-1);
      }
    } else if (bytesRecvd == 0) {
      /* is this a success or failure if -
       * 1. we recvd all that was sent by the other side-  but it was less than what we asked
       * 2. we were looking for a terminator, but did nt find it 
       */
      NSDL2_API(NULL, cptr,"got 0 bytes, closing connection");        
      if (terminator) {
        Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_NOT_DONE);
        return(-1);
      } else if (offset == 0){  //nothing recvd at all
        Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_NOT_DONE);
        return(-1);
        /* we got something, though we cant be sure if we got the requested amount. however,
         * cant check against the req bytes since we could have asked for some arbitrary amt
         */  
      } else { 
        return(0);
      }
    }

    // We have data. reset the timeout to the timeout used after data has started coming in
    if (!offset) {
      now = get_ms_stamp();
      cptr->first_byte_rcv_time = now - cptr->ns_component_start_time_stamp; //Calculate first byte receive time diff
      cptr->ns_component_start_time_stamp = now;//Update ns_component_start_time_stamp

      SET_MIN (average_time->url_frst_byte_rcv_min_time, cptr->first_byte_rcv_time);
      SET_MAX (average_time->url_frst_byte_rcv_max_time, cptr->first_byte_rcv_time);
      average_time->url_frst_byte_rcv_tot_time += cptr->first_byte_rcv_time;
      average_time->url_frst_byte_rcv_count++;

      NSDL2_API(NULL, cptr, "Calculate first byte receive time diff = %d, update ns_component_start_time_stamp = %u", cptr->first_byte_rcv_time, cptr->ns_component_start_time_stamp);
      add_user_socket_recv2_timer(cptr); 
      NSDL2_API(NULL, cptr, "resetting timeout after initial read to %d vptr %p cptr %p fd %d ",cptr->timer_ptr->actual_timeout, vptr, cptr, cptr->conn_fd);
    }

    /* log what we have and check for terminator if it was passed in */
#ifdef NS_DEBUG_ON
    debug_log_user_socket_data (cptr, recvBuf+offset, bytesRecvd, 0, 0);
#endif
    if (terminator) {
      if (binaryTerm) {   //input pattern should be something like --\\x78\\x2e\\x73\\x6f\\2e
        iTerm += 2; //skip \x
        //2 byte buffer holds 2 nibbles - each is a byte when in string form (eg., AB)
        byteBuf[0] = (u_char)terminator[iTerm]; 
        byteBuf[1] = (u_char)terminator[iTerm+1]; 
        byteBuf[2] = 0;
        byteVal = htoi(byteBuf);    //convert this to a value (eg., AB = 171 in dec)
        //NSDL2_API(NULL, NULL, "byteBuf %s byteVal %2x recvBuf[%d] %2x\n",byteBuf, byteVal, offset, recvBuf[offset] & 0x2);
        if (byteVal ==  recvBuf[offset]) {
          iTerm += 2;   //for 2 bytes
          if (terminator[iTerm] == 0) {
            NSDL2_API(NULL, NULL, "matched binary terminator string %s at offset %d\n",terminator, offset);
            NSDL2_API(NULL, cptr,"matched binary terminator string %s at offset %d\n",terminator, offset);
            matchFound =1; 
          }
        } else  //no match, start again
          iTerm =0;    
      
      } else {
        //check the byte we recvd
        if (terminator[iTerm] ==  recvBuf[offset]) {
          iTerm++;
          if (terminator[iTerm] == 0) {  //string terminator
            NSDL2_API(NULL, NULL, "matched terminator string %s at offset %d\n",terminator, offset);
            NSDL2_API(NULL, cptr,"matched terminator string %s at offset %d\n",terminator, offset);
            matchFound =1; 
          } else  //no match, start again
            iTerm =0;    
        }
      }
    }
    offset += bytesRecvd; 
    bytesLeft -=  bytesRecvd;
    ((userSocketData*)cptr->data)->recvBuf = recvBuf;
    ((userSocketData*)cptr->data)->recvBufBytes = offset;
    //stats for bytes read
    //user_socket_avgtime->user_socket_rx_bytes += bytesRecvd;
    //user_socket_avgtime->user_socket_total_bytes += bytesRecvd;


    NSDL2_API(NULL, cptr, "cptr %p fd %d recvBufSize %d bytesRecvd %d bytesLeft %d  offset %d count %d matchfound %d",cptr, cptr->conn_fd, recvBufSize, bytesRecvd, bytesLeft, offset, count, matchFound);

    /* when we're looking for a terminator,  and the size to recv was not provided, we recv 
     * in increments of NS_RECV_BUF_SIZE, looking for the terminator until timeout or error. So,
     * increment the left over bytes marker for the next recv batch now for this 
     */
    if (terminator) {
      if (matchFound) {
        NSDL2_API(NULL, cptr, "found match for terminator string, not recv'ing any more");
        break;    //terminator found
      }
      if (timeoutFlag)          //recv until timeout
        bytesLeft = NS_RECV_BUFSIZE;
      else
        if (!bytesLeft)  {
          NSDL2_API(NULL, cptr, "recvd max possible but found no terminator, not recv'ing any more");
          break;    //recvd max possible but found no terminator
        }
    } else {
      /* we check this before case below this because bytesLeft may be 0 based on the size we
       *started with (which is always fixed). but we cant stop if there was no size specified
       * and also no other terminate conditions.
       */
      if (timeoutFlag)  { 
        bytesLeft = NS_RECV_BUFSIZE;
        continue;
      }
      if (!bytesLeft) {
        NSDL2_API(NULL, cptr, "recvd max possible, no more to recv");
        break;    //recvd what we were supposed to, w/o terminator
      }
    }

    /* 
     * we've considered all the cases that will result in breaking out of the recv loop above
     * should we loop again or return to epoll ?
     * we did not get here due to EAGAIN, but most probably because we recvd a chunk of data
     * that we asked for. Note that In ET, epoll is and must be considered ready (hence it will 
     * not notify us of an event) unless the I/O space is exhausted. So, we must loop until the
     * pipe is drained
     */

  }   //while

  // delete timeout timers - commenting this. Close_conn deletes the timer
  //delete_user_socket_timeout_timer(cptr);
  if (terminator && !matchFound) {
    NSEL_MAJ(NULL, cptr, ERROR_ID, ERROR_ATTR, "cptr %p fd %d recvd %d bytes but didnt find terminator (%s)",cptr, cptr->conn_fd, offset, terminator); 
    Close_connection(cptr, 0, now, NS_REQUEST_INCOMPLETE_EXACT, NS_COMPLETION_EXACT);
    return(-1);
  }
  cptr->request_complete_time = now = get_ms_stamp();

  SET_MIN (average_time->url_dwnld_min_time, cptr->request_complete_time);
  SET_MAX (average_time->url_dwnld_max_time, cptr->request_complete_time);
  average_time->url_dwnld_tot_time += cptr->request_complete_time;
  average_time->url_dwnld_count++;

  NSDL2_API(NULL, cptr, "receive complete or errored out. cptr %p fd %d last offset %d", cptr, cptr->conn_fd, offset);
  return(0);
}

/*
 * description:
 * Receives data of a specified length from a datagram or stream socket. 
 *
 * Inputs
 *
 * char *s_desc           descriptor identifying connected socket
 * char *bufindex         buffer 
 * Optional - 
 * [char *flags,]         recv and send flags in the format "Flags =<flags>"
 * [char *size,]          no of bytes to recv in the format "NumberOfBytesToRecv=xx"
 * [char *terminator,]    The character(s) marking the end of the block to receive, in the 
 *                        format:"StringTerminator= value " or 
 *                              "BinaryStringTerminator= value " 
 *                          (only available for TCP sockets). 
 * [char *mismatch,]      The criterion for a mismatch—size or content. Use the following 
 *                        format: "Mismatch= value " where value can be either MISMATCH_SIZE 
 *                        (default) or 
 *                        MISMATCH_CONTENT
 * [char *RecordingSize,] The size of the buffer received during recording (only 
 *                        available for TCP sockets). Use the following format: 
 *                        "RecordingSize" 
 * NSLastarg              A marker indicating the end of the parameters (used where optional 
 *                        parameters are available). 
 *
 * Outputs
 *
 * Algo
 *
 * Errors
 */

#if RUN_TIME_PARSER
  int 
ns_sock_receive_ex (char *socket_desc, char* buf_name, ...)
  char *p, name;
  va_list ap;
  int recvMismatchSizeSet =0, recvMismatchContentSet=0;
#else
  int 
ns_sock_receive_ex (char *socket_desc, char* buf_name, char *recv_flags, char *recv_size, char *recvTerminator, char* recvMismatch, char* recv_rec_size)
#endif
{
  int fd;
  //int recvFlagsSet =0;
  //int recvSizeSet=0;
  //int recvRecSizeSet;
  int  recvTerminatorSet=0, recvMismatchSet =0, recvUntilTO =0 ;
  int recvSize =0, recvRecSize =0, recvFlags =0, nBytes;
  u_ns_ts_t now = get_ms_stamp();

  connection *cptr;
  //extract optional args as in 1.
  // these are – flags, size, terminator, mismatch, recording_size	
  // get fd from socket string
  fd = ns_get_int_val(socket_desc);

#if RUN_TIME_PARSER
  va_start (ap, buf_name);
  while (1) {
    name = va_arg(ap, char*);
    if (!strncmp(name, LASTARG_MARKER, strlen(LASTARG_MARKER))) {
      break;
    }
    count++;    //count the # of args - to avoid an infinite loop
    if (count > NS_RECV_EX_MAX_VARARGS) {
      ERROR("last arg marker %s not found. stop processing args\n",LASTARG_MARKER);
      return(-1);
    }
    if (!strncmp(name, "Flags", strlen("Flags"))) {
      //recvFlagsSet = 1;
      if ( (p = getNextToken(name,'=')) == NULL) {
        ERROR("no value found after Flags token\n");
        return(-1);
      }
      recvFlags = atoi(p);
      NSDL2_API(NULL, NULL, "recvFlags = 0x%x\n",recvFlags); 
    } else if (!strncmp(name, "NumberOfBytesToRecv", strlen("NumberOfBytesToRecv"))) {
      //recvSizeSet = 1;
      if ( (p = getNextToken(name,'=')) == NULL) {
        ERROR("no value found after NumberOfBytesToRecv token\n");
        return(-1);
      }
      recvSize = atoi(p);
      NSDL2_API(NULL, NULL, "recvSize = %d\n",recvSize); 
    } else if (!strncmp(name, "StringTerminator", strlen("StringTerminator")) ||
      !strncmp(name, "BinaryStringTerminator", strlen("BinaryStringTerminator"))) {
      recvTerminatorSet = 1;
      if ( (recvTerminator = getNextToken(name,'=')) == NULL) {
        ERROR("no value found after \"StringTerminator=\" or \"BinaryStringTerminator=\" token\n");
        return(-1);
      }
      //only valid for TCP
      if ( ((userSocketData*)cptr->data)->sockType != SOCK_STREAM) {
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"%d is not a TCP socket",fd);
        return(-1);
      } else if (cptr->conn_state != CNST_CONNECTED) {
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"TCP socket %d not connected\n",fd);
        return(-1);
      }
 
      NSDL2_API(NULL, NULL, "recvTerminator = %s\n",recvTerminator); 
    } else if (!strncmp(name, "Mismatch", strlen("Mismatch"))) {
      recvMismatchSet = 1;
      if ( (recvMismatch = getNextToken(name,'=')) == NULL) {
        ERROR("no value found after \"Mismatch =\" or \"Mismatch =\" token\n");
        return(-1);
      }
      NSDL2_API(NULL, NULL, "recvMismatch = %s\n",recvMismatch); 
      if (!strncmp(recvMismatch, "MISMATCH_SIZE", strlen("MISMATCH_SIZE"))) {
        recvMismatchSizeSet = 1;
      } else if (!strncmp(recvMismatch, "MISMATCH_CONTENT", strlen("MISMATCH_CONTENT"))) {
        recvMismatchContentSet = 1;
      } else {
        ERROR("invalid value for recvMismatch %s\n",recvMismatch);
      }
    } else if (!strncmp(name, "RecordingSize", strlen("RecordingSize"))) {
      //recvRecSizeSet = 1;
      if ( (p = getNextToken(name,'=')) == NULL) {
        ERROR("no value found after \"RecordingSize=\" \n");
        return(-1);
      }
      recvRecSize = atoi(p);
      NSDL2_API(NULL, NULL, "recvRecSize = %s\n",recvRecSize); 
    } else {
      ERROR("illegal arg %s\n",name);
      return(-1);
    }
  }   //while(1)
  va_end(ap);  
 

#else     //RUN_TIME_PARSER
  if (recv_flags) {
    //recvFlagsSet = 1;
    recvFlags = atoi(recv_flags);
    NSDL2_API(NULL, NULL, "recvFlags = 0x%x\n",recvFlags); 
  }
  if (recv_size) {
    //recvSizeSet = 1;
    recvSize = atoi(recv_size);
    NSDL2_API(NULL, NULL, "recvSize = %d\n",recvSize); 
  }
  if (recvTerminator) {
    recvTerminatorSet = 1;
    NSDL2_API(NULL, NULL, "recvTerminator = %s\n",recvTerminator); 
  }
  if (recvMismatch) {
    recvMismatchSet = 1;
    NSDL2_API(NULL, NULL, "recvMismatch = %s\n",recvMismatch); 
  }
  
  if (recv_rec_size) {
    //recvRecSizeSet = 1;
    recvRecSize = atoi(recv_rec_size);
    NSDL2_API(NULL, NULL, "recvRecSize = %d\n",recvRecSize); 
  }
#endif

  //recv() /recvfrom() can both be used 
  if (recvSize && recvRecSize)
    nBytes = recvSize;
  else if (recvSize) 
    nBytes = recvSize;
  else	if (recvRecSize) 
    nBytes = recvRecSize;
  else if (recvTerminatorSet || recvMismatchSet) { //recieve everything until terminator found
    recvUntilTO = 1;    // terminator set and max size to recv not specified
    nBytes = NS_RECV_BUFSIZE;   //start with this initially. adjust later
  } else {              //recv until TO - same as above, but separated for clarity
    recvUntilTO = 1;
    nBytes = NS_RECV_BUFSIZE;   //start with this initially. adjust later
  }
    
  if ( (cptr = fd_to_cptr(fd)) == NULL) {
    SET_PAGE_AND_SESSION_ERROR  
    return(-1); 
  }


  /* For UDP receive, we need local, but remote is not compulsory
   * this check is not reliable unless these fields were set to NULL
   */

  #ifdef NS_DEBUG_ON
  if ( ((userSocketData*)cptr->data)->sockType == SOCK_DGRAM) {  //UDP
    //checking only port now- can check address in addition to this for better results
    int localPort;
    int remotePort;
    localPort = ntohs(cptr->sin.sin6_port);
    remotePort = ntohs(cptr->cur_server.sin6_port);
    NSDL2_API(NULL, NULL, "UDP localPort %d remotePort %d\n",localPort, remotePort); 
  } 
  #endif


  /*
   * For TCP receive
   * if socket was used to listen - we can just do a recv on this socket. we get anything 
   * that is sent to this -socket must be in 
   * if socket was used to connect, again we can just recv
   * socket must be in 
   */

  if ( ((userSocketData*)cptr->data)->sockType == SOCK_STREAM) {  //TCP
    if ( !( (cptr->conn_state == CNST_ACCEPTED) || (cptr->conn_state == CNST_CONNECTED) || 
          (cptr->conn_state == CNST_READING)) ) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"TCP socket %d is not in right state for recv, state is 0x%x",cptr->conn_fd, cptr->conn_state);
      goto err;
    }
  }

  if (ns_user_socket_handle_read(cptr, nBytes, recvTerminator, recvMismatch, recvUntilTO, recvFlags) == -1) {
    return(-1);
  }

  //save the recvd buffer into buffer name passed in 
  ns_save_string_flag(((userSocketData*)cptr->data)->recvBuf, ((userSocketData*)cptr->data)->recvBufBytes, buf_name, 0);
  //if there are no errors, leave connection open as user may use it again
   //Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);

  return(0);
err:
  Close_connection(cptr, 0, now, NS_REQUEST_ERRMISC, NS_COMPLETION_NOT_DONE);
  return(-1);
}

/*
 * description:
 * Receives data from a datagram or stream socket. 
 *
 * Inputs
 *
 * char *s_desc           descriptor identifying connected socket
 * char *bufindex         buffer 
 * Optional - 
 * [char *flags,]         recv and send flags in the format "Flags =<flags>"
 * NSLastarg              A marker indicating the end of the parameters (used where optional 
 *                        parameters are available). 
 *
 * Outputs
 *
 * Algo
 *
 * Errors
 */

#if RUN_TIME_PARSER
  int 
ns_sock_receive(char *socket_desc, char* buf_name, ...)
  char *p, name;
  va_list ap;
#else
  int 
ns_sock_receive(char *socket_desc, char* buf_name, char *recv_flags)
#endif
{
  int fd;
  //int recvFlagsSet =0;
  int recvUntilTO =0 ;
  int recvFlags =0, nBytes;

  NSDL2_API(NULL, NULL, "Method called, socket_desc = %s, buf_name = %s", socket_desc, buf_name);

  u_ns_ts_t now = get_ms_stamp();
  connection *cptr;

  fd = ns_get_int_val(socket_desc);
  NSDL2_API(NULL, NULL, "fd = %d", fd);

#if RUN_TIME_PARSER
  va_start (ap, buf_name);
  while (1) {
    name = va_arg(ap, char*);
    if (!strncmp(name, LASTARG_MARKER, strlen(LASTARG_MARKER))) {
      break;
    }
    count++;    //count the # of args - to avoid an infinite loop
    if (count > NS_RECV_MAX_VARARGS) {
      ERROR("last arg marker %s not found. stop processing args\n",LASTARG_MARKER);
      return(-1);
    }
    if (!strncmp(name, "Flags", strlen("Flags"))) {
      //recvFlagsSet = 1;
      if ( (p = getNextToken(name,'=')) == NULL) {
        ERROR("no value found after Flags token\n");
        return(-1);
      }
      recvFlags = atoi(p);
      NSDL2_API(NULL, NULL, "recvFlags = 0x%x\n",recvFlags); 
    } else {
      ERROR("illegal arg %s\n",name);
      return(-1);
    }
  }   //while(1)
  va_end(ap);  
#else     //RUN_TIME_PARSER
  if (recv_flags) {
    //recvFlagsSet = 1;
    recvFlags = atoi(recv_flags);
    NSDL2_API(NULL, NULL, "recvFlags = 0x%x\n",recvFlags); 
  }
#endif
  recvUntilTO = 1;    // terminator set and max size to recv not specified
  nBytes = NS_RECV_BUFSIZE;   //start with this initially. check inside 
   
  if ( (cptr = fd_to_cptr(fd)) == NULL) {
    SET_PAGE_AND_SESSION_ERROR  
    return(-1); 
  }

  cptr->conn_state = CNST_READING;
  NSDL2_API(NULL, NULL, "cptr = %p, sockType = %d, conn_state = %d", 
                         cptr, ((userSocketData*)cptr->data)->sockType, cptr->conn_state);
  /* For UDP receive, we need local, but remote is not compulsory
   * this check is not reliable unless these fields were set to NULL
   */

  #ifdef NS_DEBUG_ON
  if ( ((userSocketData*)cptr->data)->sockType == SOCK_DGRAM) {  //UDP
    //checking only port now- can check address in addition to this for better results
    int localPort, remotePort;
    localPort = ntohs(cptr->sin.sin6_port);
    remotePort = ntohs(cptr->cur_server.sin6_port);
    NSDL2_API(NULL, NULL, "UDP localPort %d remotePort %d\n",localPort, remotePort); 
  } 
  #endif


//TCP must have a connection
  if ( ((userSocketData*)cptr->data)->sockType == SOCK_STREAM) {  //TCP
    if ( !( (cptr->conn_state == CNST_ACCEPTED) || (cptr->conn_state == CNST_CONNECTED) || 
          (cptr->conn_state == CNST_READING)) ) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"TCP socket %d is not in right state for recv, state is 0x%x",cptr->conn_fd, cptr->conn_state);
      goto err;
    }
  }

  if (ns_user_socket_handle_read(cptr, nBytes, NULL, NULL, recvUntilTO, recvFlags) == -1) {
    NSDL2_API(NULL, NULL, "Returning ..");
    return(-1);
  }

  NSDL2_API(NULL, NULL, "recvBufBytes = %d", ((userSocketData*)cptr->data)->recvBufBytes);
  //save the recvd buffer into buffer name passed in 
  if(((userSocketData*)cptr->data)->recvBufBytes)
    ns_save_string_flag(((userSocketData*)cptr->data)->recvBuf, ((userSocketData*)cptr->data)->recvBufBytes, buf_name, 0);
  //if there are no errors, leave connection open as user may use it again
  //Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);

  return(0);

err:
  NSDL2_API(NULL, NULL, "Method exit.");
  Close_connection(cptr, 0, now, NS_REQUEST_ERRMISC, NS_COMPLETION_NOT_DONE);
  return(-1);
}


/* 
 * description
 * retrieves socket attributes
 *
 * Inputs 
 * char *socket_desc      descriptor identifying a socket 
 * int attribute          a socket attribute
 * valid attributes are -
 * LOCAL_ADDRESS          The IP address (in standard dotted form) of the machine running the 
 *                        script.  
 * LOCAL_HOSTNAME         The host name of the local machine. 
 * LOCAL_PORT             The port number of the socket in the host byte order. This may 
 *                        be used for either bounded or connected sockets. 
 * REMOTE_ADDRESS         The IP address of the peer machine. (TCP sockets only) 
 * REMOTE_HOSTNAME        The host name of the peer machine. (TCP sockets only) 
 * REMOTE_PORT            The port number of the peer socket in the host byte order. 
 *                        (TCP sockets only) 
 *
 * Note
 * we return a pointer to statically allocated buffer or NULL (on error)
 * or from another library call. this could get overwritten on subsequent calls
 */

char 
*ns_sock_get_socket_attrib (char *socket_desc , int attribute)
{
	struct sockaddr_in name;
  int fd;
  connection *cptr;
  socklen_t nameLen;
  u_short port;
  static char localHost[NI_MAXHOST], remoteHost[NI_MAXHOST], service[NI_MAXSERV];
  static char portBuf[6];   //max port = 65535, so 5+1 null bytes to store this

  // get fd from socket name
  fd = ns_get_int_val(socket_desc);
  if ( (cptr = fd_to_cptr(fd)) == NULL) {
    SET_PAGE_AND_SESSION_ERROR  
    return(NULL);
  }

  switch (attribute) {
    case LOCAL_ADDRESS: 
    case LOCAL_HOSTNAME:
    case LOCAL_PORT:
		nameLen = sizeof(name);
    if (getsockname(fd, (struct sockaddr*)&name, &nameLen) == -1) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "getsockname failed. errno %d (%s)",errno, nslb_strerror(errno));
      return(NULL);
    }
    
    if (attribute == LOCAL_ADDRESS) {
      //convert address to dotted quad notation
      return (inet_ntoa (name.sin_addr));
    } else if (attribute == LOCAL_HOSTNAME) {
      if ( (addrToName((struct sockaddr*)&name, nameLen, localHost, sizeof(localHost), service, sizeof(service))) == -1)
        return (NULL);
      return(localHost);
    } else if (attribute == LOCAL_PORT) {
      port = ntohs(name.sin_port);
      memset(portBuf, 0, sizeof(portBuf));
      //copy sizeof(portBuf) bytes because it includes NULL byte - see snprintf man page
      snprintf(portBuf, sizeof(portBuf), "%d", port); 
      return(portBuf);
    }
    break;
    case REMOTE_ADDRESS:		
    case REMOTE_HOSTNAME:
    case REMOTE_PORT:
    //verify that socket is a TCP socket and is connected for these 
    if ( ((userSocketData*)cptr->data)->sockType != SOCK_STREAM) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"%d is not a TCP socket",fd);
      return(NULL);
    } else if (cptr->conn_state != CNST_CONNECTED) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"TCP socket %d not connected\n",fd);
      return(NULL);
    }
 
		nameLen = sizeof(name);
    if (getpeername(fd, (struct sockaddr*)&name, &nameLen) == -1) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "getpeername failed. errno %d (%s)",errno, nslb_strerror(errno));
      return(NULL);
    }
    if (attribute == REMOTE_ADDRESS) {
      return (inet_ntoa (name.sin_addr));
    }	else if (attribute == REMOTE_HOSTNAME) {
      if ( (addrToName((struct sockaddr*)&name, nameLen, remoteHost, sizeof(remoteHost), service, sizeof(service))) == -1) 
        return (NULL);
      return(remoteHost);
    } else if (attribute == REMOTE_PORT) {
      port = ntohs(name.sin_port);
      memset(portBuf, 0, sizeof(portBuf));
      //copy sizeof(portBuf) bytes because it includes NULL byte - see snprintf man page
      snprintf(portBuf, sizeof(portBuf), "%d", port);
      return(portBuf);
    }
    break;
    default:
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "invalid attribute %d",attribute);
      return(NULL);
  } //switch
  //shouldnt get here
	return(NULL);	
}

#define NS_SOCK_CREATE_SOCKET_PAT "\\<ns_sock_create_socket\\>[ \t]*\\(.*\\)"
#define NS_SOCK_SEND_PAT "\\<ns_sock_send\\>[ \t]*\\(.*\\)"
#define NS_SOCK_RECV_EX_PAT "\\<ns_sock_receive_ex\\>[ \t]*\\(.*\\)"
#define NS_SOCK_RECV_PAT "\\<ns_sock_receive\\>[ \t]*\\(.*\\)"

/*
 * -1 = error
 *  1 = no error, but no match
 *  0 = match
 */
static int 
match_api_name(char* line, char *pat, regmatch_t *pmatch[])
{
  regex_t regex;
  int status, nmatch;
  static regmatch_t match[2];
  char errbuf[1024];

  if ( (status = regcomp(&regex, pat, REG_ICASE|REG_EXTENDED)) != 0) {
    regerror(status, &regex, errbuf, sizeof(errbuf));
    ERROR ("regcomp error: %s\n",errbuf);
    return(-1);
  }


  nmatch =1;
  if ( (status = regexec(&regex, line, nmatch, match, 0)) == REG_NOMATCH) {
    //NSDL2_API(NULL, NULL, "no match\n");
    return(1);
  }
  //DEBUG ("pattern matched from [%s] to [%s] \n",line+match[0].rm_so, line+match[0].rm_eo);
  pmatch[0] = &match[0];
  regfree(&regex);
  return(0);
}



/*
 * Can be used to extract api name if used give a general name to it
 * api_name_part - unique substring of the full api name to search, starting from the begining
 * api_name - full api name to return
 */

static char* 
extract_api_name(const char *line, const char* api_name_part, char *api_name, int max_len, int *bytes_parsed) {

  char *line_ptr;
  char *ptr;
  int len;

  line_ptr = strstr(line, api_name_part);

  if(!line_ptr) {
    //NSDL2_API(NULL, NULL, "could not find api name starting with %s in line = [%s]", api_name_part, line);
    ERROR ("could not find api name starting with %s in line = [%s]\n", api_name_part, line);
    return NULL;
  }
  *bytes_parsed = line_ptr -line;

  // NSDL2_API(NULL, NULL, "Method Called, line_ptr = [%s]", line_ptr);
  //DEBUG ("Method Called, line_ptr = [%s], bytes %d\n", line_ptr, *bytes_parsed);
  ptr = index(line_ptr, '(');

  if(ptr) {
   len = ptr - line_ptr; 
   strncpy(api_name, line_ptr, len);
   api_name[len] = '\0';
   *bytes_parsed += len;
   //NSDL2_API (NULL, NULL, "Parsed api_name = [%s]", api_name);
   //DEBUG ("Parsed api_name %s len %d bytes %d\n", api_name, len, *bytes_parsed);
   return api_name;
  } else {
    return NULL;
  }
}

/*
 * strip a buffer in place of all quotes
 * esc - 1/0 (1 = dont remove quote if escaped, 0 - remove all quotes)
 */

int
removeQuotes(char *buf, char c, char esc)
{
  char *p, *p1;
  p = p1 = buf;
  char esc_flag =0;

  if (ESCAPE_CHAR == c) {
    ERROR("ecape char (%c) and char to be removed (%c) are same\n", ESCAPE_CHAR,c);
    return(-1);
  }
  while(*p1) {
    if (*p1 != c || esc_flag) {
     *p++ = *p1;
      if (esc && *p1 == ESCAPE_CHAR) 
        esc_flag =1;
      else
        esc_flag =0;
    } 
    p1++;
  } 
  *p =0;
  return(0);
}

#if 0
static int 
get_tokens(char *read_buf, char *fields[], char *token )
{
  int  totalFlds = 0;
  char *ptr;

  //NSDL2_API(NULL, NULL, "buf = %s token = %s\n",read_buf, token); 

  ptr = read_buf;
  while((fields[totalFlds] = strtok(ptr, token)) != NULL)
  {
    ptr = NULL;
    totalFlds++;
  }
#if 0
  {
    int i;
    NSDL2_API(NULL, NULL, "returning #fields %d\n",totalFlds);
    for (i=0; i<totalFlds; i++)
      printf("%s\n",fields[i]);
  }
#endif
  return(totalFlds);
}
#endif

/*
 * extract the value from name =value pair
 * input  
 * name_value_pairs - array containing name/value pairs
 * delim - usually '='
 * nval - no of name/value pairs
 * output
 * value 
 */

static int
get_value(char **name_value_pairs,  int nval, char delim, char *name, char* value)
{
  int i;
  //NSDL2_API(NULL, NULL, "name %s delim %c\n",name, delim);
  char *pval, *pdelim, *ptemp, *pname_value;
  for (i=0; i< nval; i++){
    //skip leading space before the name, otherwise strcmp will fail for name
    pname_value = name_value_pairs[i];
    SKIP_SPACE_NL(pname_value); 
    pdelim = strchr(pname_value, delim);
    if (pdelim == NULL) continue;
    ptemp =pdelim;
    while (isspace(*(ptemp-1))) ptemp--;      //skip space before delim
    if (!strncasecmp(pname_value, name, ptemp-pname_value) ) {
      pdelim++;
      SKIP_SPACE(pdelim);
      pval = pdelim;
      strcpy(value, pval);
      return(i);
    }
  }
  return(-1);
}


//find nth occurence of c in string s
static char*
strnchr(char *s, int c, int n) 
{
  int count =0;
  //NSDL2_API(NULL, NULL, "s %s n %d\n",s, n);
  if (!s) return(NULL);
  while (*s) {  
    if (*s == c)  {
      count++;
      if (n == count) return(s);
    }
    s++;
  }
  return(NULL);
}


/* 
 * Description: parse only the apis with args that may need run time parsing if we 
 * dont do it here.
 * the apis that do not pass args of the type "name=value" can be written out as plain
 * C apis 
 */ 

int
parse_ns_socket_api(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  char *ptr = script_line, *tmp_ptr, *tmp_last_ptr, *p;
  const char *api_name_part = "ns_sock";    //dont have a unique prefix to search
  char *fields[MAX_SOCKET_API_ARGS], value[1024], value1[1024], quotValue [1024], *tmpBuf;
  int i, nf, bytes =0, offset =0, status, offsetCloseBrace;
  char script_line_buf[MAX_LINE_LENGTH + 1];
  char api_name[MAX_API_NAME_LENGTH];
  regmatch_t *pmatch[1];
  //u_char localHostSet =0;
  //u_char remoteHostSet =0;
  //u_char backlogSet =0;

  //NSDL2_API(NULL, NULL, "script line = %s\n", script_line);
  //check for presence of each api one by one in this line
  if ( (status = match_api_name(ptr, NS_SOCK_CREATE_SOCKET_PAT, pmatch)) == -1) {
    ERROR("error in regex match \n");
    return(NS_PARSE_SCRIPT_ERROR);
  } else if (status == 0) {
    goto found;
  }

  if ( (status = match_api_name(ptr, NS_SOCK_SEND_PAT, pmatch)) == -1) {
    ERROR("error in regex match \n");
    return(NS_PARSE_SCRIPT_ERROR);
  } else if (status == 0) {
    goto found;
  }

  if ( (status = match_api_name(ptr, NS_SOCK_RECV_PAT, pmatch)) == -1) {
    ERROR("error in regex match \n");
    return(NS_PARSE_SCRIPT_ERROR);
  } else if (status == 0) {
    goto found;
  }

  if ( (status = match_api_name(ptr, NS_SOCK_RECV_EX_PAT, pmatch)) == -1) {
    ERROR("error in regex match \n");
    return(NS_PARSE_SCRIPT_ERROR);
  } else if (status == 0) {
    goto found;
  }

  //no api matched - just write out what ever we have
  assert(status == 1);
  //NSDL2_API(NULL, NULL, "api call not found, writing as is to output \n");
  write_line(outfp, script_line, 0);
  return(0);

found:

  CLEAR_WHITE_SPACE(ptr);

  if (!extract_api_name(ptr, api_name_part, api_name, MAX_API_NAME_LENGTH, &bytes)) {
    ERROR("Failed to extract api name from script [%s] at line [%d]\n", 
        RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), script_ln_no);
    return NS_PARSE_SCRIPT_ERROR;
  }
  
  //NSDL2_API(NULL, NULL, "found api %s\n",api_name);
  ptr = ptr + bytes +1;   // +1 for the opening brace which we didnt step over
  /* offset refers to the # of bytes from the begining of script_line, and also to the offset
   * in the output buffer - as long we copy everything, which is true until the fixed args
   * in the api. 
   */
  offset = ptr - script_line;   //upto and 1 byte after opening brace here
  tmpBuf = strdupa(ptr);
  if ( (tmp_ptr = strchr(tmpBuf, ')')) == NULL) {
    ERROR("closing brace not found in api call \n");
    return(NS_PARSE_SCRIPT_ERROR);
  }
  *tmp_ptr = 0;

  //NSDL2_API(NULL, NULL, "tmpBuf (before unquoting args) %s\n",tmpBuf);
  // keep offset of closing brace in script_line - we need this later
  offsetCloseBrace = offset + tmp_ptr - tmpBuf;

  /*
   * position tmp_ptr to the comma after the (last-1) arg, since we dont want to copy the 
   * LASTARG_MARKER
   */
  i =0;
  if (!strcmp(api_name, "ns_sock_create_socket")) {
    i = NO_FIXED_ARGS_CREATE_SOCKET -1;
  } else if (!strcmp(api_name, "ns_sock_send")) {
    i = NO_FIXED_ARGS_SEND -1;
  } else if (!strcmp(api_name, "ns_sock_receive_ex")) {
    i = NO_FIXED_ARGS_RECV_EX -1;
  } else if (!strcmp(api_name, "ns_sock_receive")) {
    i = NO_FIXED_ARGS_RECV -1;
  } else {
    ERROR ("invalid api %s\n",api_name);
    return NS_PARSE_SCRIPT_ERROR;
  }

  //position to point mentioned above
  if ( (tmp_last_ptr = strnchr(tmpBuf, COMMA, i)) == NULL) {
    ERROR ("couldnt find %d delimiters in %s\n",i, tmpBuf);
    return NS_PARSE_SCRIPT_ERROR;
  }
  //copy everything up to but excluding the delimiter(comma) after the last fixed arg
  offset += tmp_last_ptr - tmpBuf;
  strncpy(script_line_buf, script_line, offset);

  //NOTE: tmpBuf is altered after this call as we return the unquoted buffer in the same
  if (removeQuotes(tmpBuf, '"', 0) == -1) {
    return(NS_PARSE_SCRIPT_ERROR);
  }

  //after this point, offset is incremented based on what is written to the output buffer
  if ( (nf = get_tokens(tmpBuf, fields, COMMA_STR, MAX_SOCKET_API_ARGS)) == 0) {
    ERROR ("nf == 0 for %s\n", tmpBuf);
    return NS_PARSE_SCRIPT_ERROR;
  }
#if 0
  NSDL2_API(NULL, NULL, "get_tokens returned #fields %d\n",nf);
  for (i=0; i<nf; i++)
    printf("%s\n",fields[i]);
#endif

  if (!strcmp(api_name, "ns_sock_create_socket")) {
    //NSDL2_API(NULL, NULL, "processing api name %s\n",api_name);
    if (nf < NO_FIXED_ARGS_CREATE_SOCKET) {
      ERROR ("nf < NO_FIXED_ARGS_CREATE_SOCKET\n");
      return NS_PARSE_SCRIPT_ERROR;
    } else {
      /*
       * we may have fixed only or fixed and optional args here
       * we search for each possible arg with the name and pass NULL if its not present,
       * it doesnt matter how many args the user passed in.
       * the first 2 args are compulsory, rest are optional. The C api will have fixed 
       * positions  for these. copy only the values into arg positions. if the option is 
       * missing, we pass NULL
       * 3rd arg - LocalHost
       * 4th arg -RemoteHost
       * 5th arg - Backlog 
       * 6th arg - NSLastarg
       */
      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      if (get_value(fields, nf, EQUALS, "LocalHost", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");    //localHost value passed as NULL
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue);    //copy only value for localHost as arg
        offset += strlen(quotValue);
        //localHostSet =1;
      }

      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      if (get_value(fields, nf, EQUALS, "RemoteHost", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue);
        offset += strlen(quotValue);
        //remoteHostSet =1;
      }

      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      if (get_value(fields, nf, EQUALS, "Backlog", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue);
        offset += strlen(quotValue);
        //backlogSet =1;
      }

    }
    /*
     * sanity checks before we do anything else. cant do this at run time as netstorm still 
     * appears to run, inspite of errors from the script
     */
    //
    //both local and remote set ?
    //UDP receive needs both - so this is not an error
#if 0
    if (localHostSet && remoteHostSet) {
      ERROR("localHost and remoteHost are both set in ns_sock_create_socket()\n");
      return NS_PARSE_SCRIPT_ERROR;
    }
#endif

// copy everything after closing brace from script_line
    strcpy(script_line_buf +offset, script_line+offsetCloseBrace);
      
    //NSDL2_API(NULL, NULL, "script line output = %s\n",script_line_buf);
    write_line(outfp, script_line_buf, 1);

  } else if (!strcmp(api_name, "ns_sock_send")) {
    //NSDL2_API(NULL, NULL, "processing api name %s\n",api_name);
    if (nf < NO_FIXED_ARGS_SEND) {
      ERROR ("nf < NO_FIXED_ARGS_SEND\n");
      return NS_PARSE_SCRIPT_ERROR;
    } else {
      /*
       * 3th arg -TargetSocket
       * 4th arg - flags
       * 5th arg - NSLastarg
       */
      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      if (get_value(fields, nf, EQUALS, "TargetSocket", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue);
        offset += strlen(quotValue);
      }

      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      if (get_value(fields, nf, EQUALS, "Flags", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue);    //copy only value for localHost as arg
        offset += strlen(quotValue);
      }
    }
    // copy everything after closing brace from script_line
    strcpy(script_line_buf +offset, script_line+offsetCloseBrace);
      
    //NSDL2_API(NULL, NULL, "script line output = %s\n",script_line_buf);
    write_line(outfp, script_line_buf, 1);

  } else if (!strcmp(api_name, "ns_sock_receive_ex")) {
    if (nf < NO_FIXED_ARGS_RECV_EX) {
      ERROR ("nf < NO_FIXED_ARGS_RECV_EX\n");
      return NS_PARSE_SCRIPT_ERROR;
    } else {
      /*
       * 3th arg - recv_flags
       * 4th arg - recv_size
       * 5th arg - terminator
       * 6th arg - mismatch
       * 7th arg - recv_rec_size
       */
      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      if (get_value(fields, nf, EQUALS, "Flags", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue);
        offset += strlen(quotValue);
      }

      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      if (get_value(fields, nf, EQUALS, "NumberOfBytesToRecv", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue);
        offset += strlen(quotValue);
      }

      /* The token names for terminator could be either  "StringTerminator" or 
       * "BinaryStringTerminator" - we look for both separately, they cant both be present.
       */
      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      value[0] =0;
      value1[0] =0;

      if ( (get_value(fields, nf, EQUALS, "StringTerminator", value) == -1)  && 
          (get_value(fields, nf, EQUALS, "BinaryStringTerminator", value1) == -1))   {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        if (value[0] != 0) { //string terminator
          sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        } else if (value1[0] != 0) { //binary string terminator
          sprintf(quotValue, "\"%s\"", value1);   // this is a string, put the quotes back
          //must be in this format - \x78\x2e\x73\x6f\x2e (at run time)
          //so input must have - \\x78\\x2e ..
          for (p = value1; *p; p+=5) {    //2 backslashes + x + 2 nibbles = 5
            if ( (*p != '\\') || (*(p+1) != '\\') || (!(*(p+2) == 'x') || (*(p+2) == 'X')) ) {
              ERROR ("BinaryStringTerminator should be in this format -- \\\\x78\\\\x2e\\\\x73\\\\x6f\\\\x2e. found %s\n",p);
              return NS_PARSE_SCRIPT_ERROR;
            }
            //next 2 bytes make a hex byte. will need to convert it to check. do that later
          }
        } else { //error- we expect one of them to be non NULL
          ERROR ("expected one of them to be present -StringTerminator or BinaryStringTerminator\n");
          return NS_PARSE_SCRIPT_ERROR;
        }
        strcpy(script_line_buf+offset, quotValue); 
        offset += strlen(quotValue);
      }
      /* The token names for mismatch could be either  "MISMATCH_SIZE" or 
       * "MISMATCH_CONTENT"
       */
      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;

      if (get_value(fields, nf, EQUALS, "Mismatch", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue); 
        offset += strlen(quotValue);
      }

      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      if (get_value(fields, nf, EQUALS, "RecordingSize", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue);
        offset += strlen(quotValue);
      }

    }
    // copy everything after closing brace from script_line
    strcpy(script_line_buf +offset, script_line+offsetCloseBrace);
      
    //NSDL2_API(NULL, NULL, "script line output = %s\n",script_line_buf);
    write_line(outfp, script_line_buf, 1);
  } else if (!strcmp(api_name, "ns_sock_receive")) {
    if (nf < NO_FIXED_ARGS_RECV) {
      ERROR ("nf < NO_FIXED_ARGS_RECV\n");
      return NS_PARSE_SCRIPT_ERROR;
    } else {
      /*
       * 3th arg - recv_flags
       */
      strncpy(script_line_buf +offset, ", ", 2);
      offset += 2;
      if (get_value(fields, nf, EQUALS, "Flags", value) == -1) {
        strcpy(script_line_buf+offset, "NULL");
        offset += 4;
      } else {
        sprintf(quotValue, "\"%s\"", value);   // this is a string, put the quotes back
        strcpy(script_line_buf+offset, quotValue);
        offset += strlen(quotValue);
      }
    }
    // copy everything after closing brace from script_line
    strcpy(script_line_buf +offset, script_line+offsetCloseBrace);
      
    //NSDL2_API(NULL, NULL, "script line output = %s\n",script_line_buf);
    write_line(outfp, script_line_buf, 1);
  } 

  // Added below code since getting core at the time of logging
  char pagename[MAX_LINE_LENGTH];
  char *flow_name = NULL;
  char flow_file_lol[4096];
  int index =0;

  sprintf(flow_file_lol, "%s", flow_filename);
  //Allocating record for new page 
  if(create_page_table_entry(&g_cur_page) != SUCCESS)
  SCRIPT_PARSE_ERROR(script_line, "Unable to create page table");
  
  strcpy(pagename, "sock_api_pagename");
  //Copy page name into big buffer
  if ((gPageTable[g_cur_page].page_name = copy_into_big_buf(pagename, 0)) == -1)
  SCRIPT_PARSE_ERROR(script_line, "Failed to copy pagename into big buffer");
  
  //Extract flow name from flow_file which includes path n name of the flow file
  flow_name = basename(flow_file_lol);
  NSDL3_PARSING(NULL, NULL, "flow_name = %s", flow_name);
  
  //Copy flow file into big buffer
  if ((gPageTable[g_cur_page].flow_name = copy_into_big_buf(flow_name, 0)) == -1)
  SCRIPT_PARSE_ERROR(script_line, "Failed to copy flowname into big buffer");
  
  //Setting values for new page in page structure
  if (gSessionTable[sess_idx].num_pages == 0)
  {
    gSessionTable[sess_idx].first_page = g_cur_page;
    NSDL2_PARSING(NULL, NULL, "Current Page Number = %d", g_cur_page);
  }
  
  gSessionTable[sess_idx].num_pages++;
  
  gPageTable[g_cur_page].first_eurl = index;
  gPageTable[g_cur_page].num_eurls++; // Increment urls

  NSDL2_PARSING(NULL, NULL, "Number of Pages for socket api script= %d num_eurls %d ", 
                             gSessionTable[sess_idx].num_pages, gPageTable[g_cur_page].num_eurls);
  return(0);
}
