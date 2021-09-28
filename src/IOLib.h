#ifndef _IOL_h
#define _IOL_h

#include<string.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define INVALID_TYPE -1
#define NO_DATA_TO_WRITE -2
#define NULL_DESTINATION -3
#define SUCCESS 1
#define ERROR 0

#define KEYFILE "proxy_1_1.pem"
#define PASSWORD "password"
#define DHFILE "dh1024_1_1.pem"
//#define PROXY_PORT 4444

	
	extern int _getline(char *buf, int size, void *src, int src_type, int req_rep);
	extern int dataread(char * buf, int size, void * src, int src_type);
	
	int putline (char *buf, void* dest, int dest_type);
	int datawrite(char * buf, int size, void *dest,int dest_type);
	void close_src_dst(void *src, void *dest,int dest_type);
	//<amitabh>
	//void * ssl_setup (int fd, char *host, char*url);
	void * ssl_setup (int fd, char *host, char*url,int proxy_mode, int sub_child_id);
	//</amitabh>
	void * ssl_set_remote (int rfd);
        void * ssl_set_clients(int rfd);
	void ssl_end();
	//<amitabh>
	int proxy_chain(int remote_server_fd, char *cur_host_name);
	//</amitabh>
	 
#endif
