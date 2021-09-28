#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include "cookies.h"
#include "ns_alloc.h"
#include "ns_log.h"

extern void end_test_run( void );

#define is_white_space(c) (((c) == ' ' || (c) == '\t') ? 1 : 0 )
#define JUNK 1
#define LINESIZE 200

int readcookies(FILE* cookiefd, char *mycookie[])
{  
  int count = 0;
  char linebuf[LINESIZE];
  char* linefeed;
  while(fgets(linebuf,LINESIZE,cookiefd) != '\0') {
    MY_MALLOC(mycookie[count] , LINESIZE+1, "mycookie[count] ", -1);
    mycookie[LINESIZE]='\0';
    strncpy(mycookie[count] , linebuf,LINESIZE);
    linefeed=strchr(mycookie[count] , 10);
    if(linefeed) *linefeed = '\0';
    count++;    
  }
  return (count);
}
