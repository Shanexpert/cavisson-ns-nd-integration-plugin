#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

/*-----------------------------------------------------------------------
//function to check if given ipv6 is in correct format
//returns 0 on success -1 on failure
------------------------------------------------------------------------*/
int check_format(char *addr)
{
  struct addrinfo hint, *res = NULL;
  int ret;

  memset(&hint, '\0', sizeof hint);

  hint.ai_family = PF_UNSPEC;
  hint.ai_flags = AI_NUMERICHOST;
  ret = getaddrinfo(addr, NULL, &hint, &res);
  if (ret) {
    puts("Invalid address");
    puts(gai_strerror(ret));
    return -1;
  }

  if (res->ai_family == AF_INET) {
    //printf("%s is an ipv4 address\n",addr);
    freeaddrinfo(res);
    return -1;
  }

  else if (res->ai_family == AF_INET6) {
         //printf("%s is an ipv6 address\n",addr);
         freeaddrinfo(res);
         return 0;
       }

  else {
         printf("%s is an is unknown address format %d\n",addr,res->ai_family);
         freeaddrinfo(res);
         return -1;
  }
}

int main(int argc, char * argv[])
{

  if (argc != 2){
    printf("Usage: %s <IP-Address>\n",argv[0]);
    return -1;
  }

  return (check_format(argv[1]));

}


