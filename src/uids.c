#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include "uids.h"
#include "ns_alloc.h"
#include "ns_log.h"

extern void end_test_run( void );

/*#define is_white_space(c) (((c) == ' ' || (c) == '\t') ? 1 : 0 )
  #define JUNK 1
*/
#define UIDLINESIZE 200 
int readuids(FILE* uidfd, char *uids[]) {   
  int count = 0;
  char linebuf[UIDLINESIZE];
  char* linefeed;
  rewind(uidfd);
  while(strncmp(fgets(linebuf,sizeof(linebuf), uidfd),"user-id",strlen("user-id"))<0);
  while(strncmp(fgets(linebuf, sizeof(linebuf), uidfd),"user-id",strlen("user-id"))== 0) {
    MY_MALLOC(uids[count], UIDLINESIZE + 1, "uids[count]", count);
    strncpy(uids[count] , linebuf,UIDLINESIZE);
    linefeed=strchr(uids[count] , 32);
    if(linefeed) *linefeed = '\0';
    count++; 
  }
  return count;
}
