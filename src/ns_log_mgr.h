
typedef struct
{
  int fd; // fd must be the 1st field as we are using this struct as epoll_event.data.ptr
  unsigned char state; /* Present state of the read/write */
  /* This flags is to log event if More samples are accumulated due to EAGAIN, must be initialized with 1*/ 
  signed char type; /* 0 for child/client and 1 for other types eg. tools. */

  char *ip; // Ip address in Ver:IP.port format (e.g IPV4:192.168.18.1.12345)
  /* Read related */
  char *read_buf;
  int read_offset;
  int read_bytes_remaining;
} ELMsg_com_con;

typedef struct {
  unsigned int ip_addr;
  unsigned short port;
  int num_connections;
  int num_fetches;
  int conf_fd;
  int url_fd;
  int ip_fd;
  int collect_no_eth_data; // If 1, do not collect ETH data.
} Client_data;

