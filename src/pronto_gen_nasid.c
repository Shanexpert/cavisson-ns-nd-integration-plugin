#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int next_nasid(char *nas_id, char *next_nasid) {
int byte[6]; 
int num_bytes=0;
char *ptr;
int cary=0;
int i;
	
	ptr = strtok(nas_id, ":"); 
	while (ptr) {
	    byte[num_bytes] = strtol(ptr, NULL, 16);
	    if ((byte[num_bytes] > 255) || (byte[num_bytes] < 0))
		return 1;
	    num_bytes++;
	    ptr = strtok(NULL, ":");
	}
	if (num_bytes != 6)
	    return 1;
	
	for (i=5; i >=0 ; i--) {
		cary = (byte[i] + 1)/256;
		byte[i] = (byte[i] + 1)%256;
		if (!cary) break;
	}
	sprintf (next_nasid, "%02x:%02x:%02x:%02x:%02x:%02x", byte[0], byte[1], byte[2], byte[3], byte[4], byte[5]);
 	return 0;	
}

int check_nasid(char *nas) {
int byte[6]; 
int num_bytes=0;
char *ptr;
char nas_id[128];
	
	strcpy(nas_id, nas);
	ptr = strtok(nas_id, ":"); 
	while (ptr) {
	    byte[num_bytes] = strtol(ptr, NULL, 16);
	    if ((byte[num_bytes] > 255) || (byte[num_bytes] < 0))
		return 1;
	    num_bytes++;
	    ptr = strtok(NULL, ":");
	}
	if (num_bytes != 6)
	    return 1;
	
 	return 0;	
}

int main( int argc, char *argv[])
{
int num_nasid;
char nas1[128], nas2[128];
int i;


	if (argc != 3) {
	    printf("usage: %s <start_nasid> <num_nas_id>\n", argv[0]);
	    exit (1);
	}

	num_nasid = atoi(argv[2]);
	strcpy(nas1, argv[1]);
	if (check_nasid(nas1)) {
		printf("Starting nasid <%s> is not in proper format\n", nas1);
		exit(1);
	}
	printf("%s\n", nas1);
	for (i=1; i <num_nasid; i++) {
		if (next_nasid (nas1, nas2)) {
			printf("NasID %s not in proper format\n", nas1);
			exit(1);
		}
		printf("%s\n", nas2);
		strcpy(nas1, nas2);
	}
return(0);
}
