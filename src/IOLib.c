
/**************************************************************************************************
	Author          : Amitabh Modak
	Date of Creation: 01-June-2005
	Version         : 1.0.0
    Description :	The file contains four helper functions to read /write in a file 
					(if src_type/dest_type = 1) or in an SSL session (if src_type/dest_type = 2).
					Depending on src_type/dest_type the pointer src/dest should be passed. 
					That is if src_type/dest_type = 1, then a (FILE *) file pointer
					should be passed or else if src_type/dest_type = 2, an SSL pointer(SSL *) 
					should be passed.

	Bugs	    :   More strict error handling need to be done for SSL read/write.
	
	External Files :IOLib.h
	7 sept 05: new function added proxy_chain.

**************************************************************************************************/
#include <unistd.h>
#include"IOLib.h"
#include "common.h"
#include <sys/types.h>
#include <signal.h>
#include <openssl/dh.h>
#include <openssl/ossl_typ.h>
#include <openssl/engine.h>
#include "util.h"
#include "ns_log.h"

#define NS_EXIT_VAR
#include "ns_exit.h"

typedef void (*sighandler_t)(int);

static SSL_CTX *ns_ctx = NULL;
static int s_server_session_id_context = 1;
 
int _getline(char *buf, int size, void *src, int src_type, int req_rep)
{
	
	int retcode; 
	if(src == NULL)
	{
		return NULL_DESTINATION; 
	}
	if(src_type == 1)
	{
		if(nslb_fgets(buf,size,(FILE *) src, 0) == NULL)
		{
			return ERROR;
		}
	}
	else if(src_type == 2)
	{
		SSL *ssl = NULL;
		ssl = (SSL *)src;
		char ch[2];
		int index = 0;
		do
		{
			 retcode = SSL_read(ssl,ch,1);
			 if(retcode <= 0)
				break;
			 buf[index++] = ch[0];	
			 if(ch[0] == '\r'){
                           retcode = SSL_read(ssl, ch , 1);
                           if(retcode <= 0)
                             break;
                           buf[index++] = ch[0];
                           if(ch[0] == '\n')
			     break;}
                         else if(ch[0] == '\n')
                           break;
                }while(1);
		//}while(SSL_pending(ssl));
		buf[index] = '\0';
	
		if(retcode <= 0)
		{        
                  switch (SSL_get_error(ssl, retcode)) {
   		  	case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
				fprintf(stderr, "SSL_read error: SSL_ERROR_ZERO_RETURN\n");
				return ERROR;
      			case SSL_ERROR_WANT_READ:
				fprintf(stderr, "SSL_read error: SSL_ERROR_WANT_READ\n");
				return ERROR;
			/* It can but isn't supposed to happen */
     	 		case SSL_ERROR_WANT_WRITE:
				fprintf(stderr, "SSL_read error: SSL_ERROR_WANT_WRITE\n");
				return ERROR;
         	        case SSL_ERROR_SYSCALL: //Some I/O error occurred
  		        	if (errno == EAGAIN){ // no more data available, return (it is like SSL_ERROR_WANT_READ)
	       		          fprintf(stderr, "SSL_read: No more data available, return\n"); 
	    		          return ERROR;
                                }

 			        if (errno == EINTR)
      		    		{
   		      		  fprintf(stderr, "SSL_read interrupted. Continuing...\n");
				  return ERROR;
		        	}
   		   	case SSL_ERROR_SSL: //A failure in the SSL library occurred, usually a protocol error
			/* FALLTHRU */
      		      		 ERR_print_errors_fp(stderr);
			         fprintf(stderr, "SSL_read ERROR SSL_ERROR_SSL ..., fp = %p, type = %d\n", ssl, req_rep);
		  	default:
   		       		fprintf(stderr, "SSL_read ERROR COMMON ...\n");
                       		return ERROR;
	             }
                  }
        }		
	else
	{
		return INVALID_TYPE;
	}
	return SUCCESS;
}

int dataread(char * buf, int size, void * src, int src_type)
{
	int retcode;
	int read = 0;
        int ret = 0;

	if(src == NULL)
	{
		return NULL_DESTINATION;
	}	
	if(src_type == 1)
	{
		if ((ret = fread(buf,sizeof(char),size,(FILE *)src)) <= 0)
		//if(ferror((FILE *)src))
			return ERROR;
          //printf("IN dataread Bytes read = %d\n", ret);
	}
	else if(src_type == 2)
	{
	    while (read < size) {
		retcode = SSL_read((SSL *)src, buf+read, size-read);
		if(retcode <=0)
		{
          		printf("Error in reading ssl response fp = %p\n",src);
			return ERROR;
		}
	 	read += retcode;
	    }
	}
	else
	{
		return INVALID_TYPE;
	}
	return SUCCESS;
}

int putline (char *buf, void* dest, int dest_type)
{	
	int retcode;
	int written = 0;
	if(buf == NULL)
	{
		return NO_DATA_TO_WRITE;
	}
	if(dest == NULL)
	{
		return NULL_DESTINATION;
	}
	if(dest_type == 1)
	{
		if(fputs(buf,(FILE *)dest) == EOF)
			return ERROR;
	}
	else if(dest_type == 2)
	{
		SSL *ssl = NULL;
		ssl = (SSL *)dest;
		//int index = 0;
		//char ch[2];
		int len = strlen(buf);

	    //printf("SSL out data len=%d buf=[%s]\n", len, buf);
	    while (written < len) {
                ERR_clear_error();
		retcode = SSL_write(ssl,buf+written,len-written); 
		if(retcode <= 0)
			return ERROR;
		written += retcode;
	    }
#if 0
		while(index < strlen(buf))
		{
			 ch[0] = buf[index++];
			 ch[1] = '\0';
			 retcode = SSL_write(ssl,ch,1);
			 if(retcode <= 0)
				return ERROR;
				  			 
		}
#endif
	}
	else
	{
		return INVALID_TYPE;
	}
	return SUCCESS;
}

int datawrite(char * buf, int size, void *dest, int dest_type)
{
	int retcode;
	int written = 0;
	if(buf == NULL)
	{
		return NO_DATA_TO_WRITE;
	}
	if(dest == NULL)
	{
		return NULL_DESTINATION;
	}
	if(dest_type == 1)
	{
		fwrite(buf,sizeof(char),size,(FILE *)dest);
		if(ferror((FILE *)dest))
			return ERROR;
	}
	else if(dest_type == 2)
	{	
		SSL *ssl = NULL;
		ssl = (SSL *)dest;
	
	    while (written < size) {
                ERR_clear_error();
		retcode = SSL_write(ssl,buf+written,size-written); 
		if(retcode <= 0)
			return ERROR;
		written += retcode;
	    }
	}
	else
	{
		return INVALID_TYPE;
	}
	return SUCCESS;
}

void
close_src_dst ( void* src, void *dst, int proto)
{
	if (proto == 1) {
	    fclose((FILE *)src);
	    fclose((FILE *)dst);
	} else {
	    SSL_shutdown((SSL*) src);
	    SSL_free((SSL*) src);
	    SSL_shutdown((SSL*) dst);
	    SSL_free((SSL*) dst);
	}
}

void load_dh_params(ctx,file)
  SSL_CTX *ctx;
  char *file;
  {
    DH *ret=0;
    BIO *bio;

    if ((bio=BIO_new_file(file,"r")) == NULL)
      berr_exit("Couldn't open DH file");

    ret=PEM_read_bio_DHparams(bio,NULL,NULL,NULL);
    BIO_free(bio);
    if(SSL_CTX_set_tmp_dh(ctx,ret)<0)
      berr_exit("Couldn't set DH parameters");
  }

void generate_eph_rsa_key(ctx)
  SSL_CTX *ctx;
  {
   #if OPENSSL_VERSION_NUMBER < 0x10100000L
      RSA *rsa;

      rsa=RSA_generate_key(512,RSA_F4,NULL,NULL);

      if (!SSL_CTX_set_tmp_rsa(ctx,rsa))
        berr_exit("Couldn't set RSA key");

      RSA_free(rsa);
   #else
      BIGNUM          *bne = NULL;
      RSA *rsa;
      int ret; 
 
      // 1. generate rsa key
      bne = BN_new();
      ret = BN_set_word(bne,RSA_F4);
      if(ret != 1){
        BN_free(bne);
      }
  
      rsa = RSA_new();
      ret = RSA_generate_key_ex(rsa, 2048, bne, NULL);
      if(ret != 1){
        RSA_free(rsa);
      }
    #endif
  }
    
  
int 
writestr(int sock, char *str)
{
int len=strlen(str);
int r,wrote=0;
    
    while(len){
      r=write(sock,str,len);
      if(r<=0)
        err_exit("writestr:Write error");
      len-=r;
      str+=r;
      wrote+=r;
    }

    return (wrote);
}

int 
readline(int sock, char *buf, int len)
{
int n,r;
char *ptr=buf;
    
    for(n=0;n<len;n++){
      r=read(sock,ptr,1);
      
      if(r<=0)
        err_exit("readline:Read error");

      if(*ptr=='\n'){
        *ptr=0;

        /* Strip off the CR if it's there */
        if(buf[n-1]=='\r'){
          buf[n-1]=0;
          n--;
        }

        return(n);
      }

      //*ptr++;
      ptr++;
    }

    err_exit("readline:Buffer too short");      
    return (-1);
}

int 
check_if_ready (int fd)
{
    fd_set rfd;
    struct timeval temp;
    int count;

        FD_ZERO(&rfd);
	FD_SET(fd, &rfd);
	temp.tv_sec = 2;
	temp.tv_usec = 0;
	count = select(fd+1, &rfd, NULL, NULL, &temp);
	return count;
}

//0 on success
//-1 on failure
int
tcp_proxy_handshake (int fd, char *host)
{
    //FILE *fp;
    char *ptr, *ptr2;
    int first=1;
    char buf[1024];
    
#if 0
    fp = fdopen (fd, "r+");
    if (!fp) 
      berr_exit("Couldn't get fp");
#endif

    //while (nslb_fgets(buf, 1024, fp, 0))
    if (readline(fd, buf, 1024) <= 0) {
      printf("Empty CONNECT");
      return (-1);
    }
 
    {
      if (first) {
	    first = 0;
	    
    	if((ptr=strtok(buf," "))<0) {
      	    printf("tcp_proxy_handshake: getting CONNECT");
	    return(-1);
	}
    	if((ptr=strtok(0," "))<0) {
      	    printf("tcp_proxy_handshake: getting host\n");
	    return(-1);
	}
    	strcpy(host, ptr);
	printf("host:%s, fd = %d \n", host, fd);
	//remove port, if default
	ptr2 = index(host, ':'); //Check if port is provided
	if (ptr2)  {
	    if (atoi(ptr2+1) == 443)
	        *ptr2 = '\0';
	}
	printf("Host is host:%s, fd = %d \n", host, fd);
      } 
	/*else if (!strcmp (buf, "\r\n"))
	    break;*/
    }

    /* Look for the blank line that signals end of header*/
    while(readline(fd,buf,sizeof(buf))>0) {
      ;
    }

    printf("Connect recd for fd = %d\n", fd);
    /* Now that we're connected, do the proxy request */
    sprintf(buf,"HTTP/1.1 200 OK\r\n\r\n");
    writestr(fd,buf);
#if 0
    if (fputs("HTTP 200 OK\r\n", fp) <= 0)
	printf("Could'nt write success for connect\n");
    if (fputs("\r\n", fp) <=0 )
	printf("Could'nt write success 2 for connect\n");
    fflush(fp);
    if (check_if_ready(fd) <= 0) {
	printf("no SSL handshake initiation\n");
	return -1;
    } else {
	return 0;
    }
#endif
	return 0;
}

int 
peek_url (SSL *ssl, char *ubuf)
{
char buf[1024*16];
int size=8;
int read;
char *ptr;
   
    ERR_clear_error();
    while (1) { 
      read = SSL_peek(ssl, buf, size);
      //int errorno = errno;
      if (read <= 0){
        int error;
        error = SSL_get_error(ssl, read);
        if(error == SSL_ERROR_SYSCALL){
           printf("SSL_peek error = SSL_ERROR_SYSCALL\n");
/*          if(ERR_get_error() == 0 && errorno == EINTR)
            continue;
          else
            printf("ssl_peel error = SSL_ERROR_SYSCALL and error no = %d and message = %s\n", errorno,strerror(errorno));
*/  
      }
        else 
          printf("ssl_peek error = %d",error);

	return -1;
      }
      if (buf[read-1] == '\n') {
        printf("request buffer = %s", buf);
	buf[read] = '\0';
	ptr = strtok(buf, " ");
	if (!ptr) {
	    printf("could not get method\n");
	    return -1;
	}
	ptr = strtok(NULL, " ");
	if (!ptr) {
	    printf("could not get URL\n");
	    return -1;
	}
	strcpy(ubuf, ptr);
	printf("URL is [%s]\n", ptr);
	return 0;
      } else {
	size++;
      }
    }
}
void mon_handler_ignore(int data)
{
}

int run_command(char *cmd, char *err_msg){
  sighandler_t prev_handler;
  int status; 
  char err_msg[1024] = "\0";

  prev_handler = signal(SIGCHLD, mon_handler_ignore);

  status = nslb_system(cmd,1,err_msg);   
  if(status != 0){
    fprintf(stderr, "Error in running system command. %s\n", err_msg);  
    return -1;  
  }
  (void) signal( SIGCHLD, prev_handler);
  return 0;
}

static DH *get_dh2236()
        {
        static unsigned char dh2236_p[]={
                0x0F,0x52,0xE5,0x24,0xF5,0xFA,0x9D,0xDC,0xC6,0xAB,0xE6,0x04,
                0xE4,0x20,0x89,0x8A,0xB4,0xBF,0x27,0xB5,0x4A,0x95,0x57,0xA1,
                0x06,0xE7,0x30,0x73,0x83,0x5E,0xC9,0x23,0x11,0xED,0x42,0x45,
                0xAC,0x49,0xD3,0xE3,0xF3,0x34,0x73,0xC5,0x7D,0x00,0x3C,0x86,
                0x63,0x74,0xE0,0x75,0x97,0x84,0x1D,0x0B,0x11,0xDA,0x04,0xD0,
                0xFE,0x4F,0xB0,0x37,0xDF,0x57,0x22,0x2E,0x96,0x42,0xE0,0x7C,
                0xD7,0x5E,0x46,0x29,0xAF,0xB1,0xF4,0x81,0xAF,0xFC,0x9A,0xEF,
                0xFA,0x89,0x9E,0x0A,0xFB,0x16,0xE3,0x8F,0x01,0xA2,0xC8,0xDD,
                0xB4,0x47,0x12,0xF8,0x29,0x09,0x13,0x6E,0x9D,0xA8,0xF9,0x5D,
                0x08,0x00,0x3A,0x8C,0xA7,0xFF,0x6C,0xCF,0xE3,0x7C,0x3B,0x6B,
                0xB4,0x26,0xCC,0xDA,0x89,0x93,0x01,0x73,0xA8,0x55,0x3E,0x5B,
                0x77,0x25,0x8F,0x27,0xA3,0xF1,0xBF,0x7A,0x73,0x1F,0x85,0x96,
                0x0C,0x45,0x14,0xC1,0x06,0xB7,0x1C,0x75,0xAA,0x10,0xBC,0x86,
                0x98,0x75,0x44,0x70,0xD1,0x0F,0x20,0xF4,0xAC,0x4C,0xB3,0x88,
                0x16,0x1C,0x7E,0xA3,0x27,0xE4,0xAD,0xE1,0xA1,0x85,0x4F,0x1A,
                0x22,0x0D,0x05,0x42,0x73,0x69,0x45,0xC9,0x2F,0xF7,0xC2,0x48,
                0xE3,0xCE,0x9D,0x74,0x58,0x53,0xE7,0xA7,0x82,0x18,0xD9,0x3D,
                0xAF,0xAB,0x40,0x9F,0xAA,0x4C,0x78,0x0A,0xC3,0x24,0x2D,0xDB,
                0x12,0xA9,0x54,0xE5,0x47,0x87,0xAC,0x52,0xFE,0xE8,0x3D,0x0B,
                0x56,0xED,0x9C,0x9F,0xFF,0x39,0xE5,0xE5,0xBF,0x62,0x32,0x42,
                0x08,0xAE,0x6A,0xED,0x88,0x0E,0xB3,0x1A,0x4C,0xD3,0x08,0xE4,
                0xC4,0xAA,0x2C,0xCC,0xB1,0x37,0xA5,0xC1,0xA9,0x64,0x7E,0xEB,
                0xF9,0xD3,0xF5,0x15,0x28,0xFE,0x2E,0xE2,0x7F,0xFE,0xD9,0xB9,
                0x38
};

  static unsigned char dh2236_g[]={
                0x02,
                };
        DH *dh;
        if ((dh=DH_new()) == NULL) return(NULL);

  #if OPENSSL_VERSION_NUMBER < 0x10100000L 
        dh->p=BN_bin2bn(dh2236_p,sizeof(dh2236_p),NULL);
        dh->g=BN_bin2bn(dh2236_g,sizeof(dh2236_g),NULL);
        if ((dh->p == NULL) || (dh->g == NULL))
                { DH_free(dh); return(NULL); }
  #else
        BIGNUM *p = BN_bin2bn(dh2236_p, sizeof(dh2236_p), NULL);
        BIGNUM *g = BN_bin2bn(dh2236_g, sizeof(dh2236_g), NULL);
        if (p == NULL || g == NULL)
        {
          BN_free(p);
          BN_free(g);
          DH_free(dh);
          return NULL;
        }

        // p, g are freed later by DH_free()
        if (0 == DH_set0_pqg(dh, p, NULL, g))
        {
          BN_free(p);
          BN_free(g);
          DH_free(dh);
          return NULL;
        }
    #endif
    return dh;
}

void  *
ssl_setup (int fd, char *host, char*url,int proxy_mode, int sub_child_id)
{
BIO *sbio;
SSL *ssl;
int r;
char *ptr;
char buf[1024];
char buf2[1024];
char cmd_buffer[2048];
char err_msg[2048];
int status;
    
    ptr = getenv("NS_WDIR");
    if (!ptr) {
        NS_EXIT(1, "NS_WDIR env variable must be defined");
    }
                char cer_file[512];

    sprintf (buf, "%s/cert/%s", ptr, KEYFILE);
    sprintf (buf2, "%s/cert/%s", ptr, DHFILE);

  if(proxy_mode !=3)
  {       
    printf("Calling tcp_proxy_handshake for sub child = %d\n", sub_child_id);
    //host will be filled from this method.
    if (tcp_proxy_handshake(fd, host)) 
    {
      printf("proxy_handshake failed\n");
      close(fd);
      return NULL;
    }
    char *tmp = strchr(host, ':');
    char host_buf[1024] = "0";
 
    strncpy(host_buf, host, (tmp != NULL?(tmp - host):strlen(host)));
    //fprintf(stderr, "host buff = %s, sub_child_id = %d\n", host_buf, sub_child_id); 
    sprintf(cmd_buffer, "openssl req -sha256 -new -out /tmp/%s_%d.csr -key %s/cert/proxy.key -subj /CN=%s", host, sub_child_id, ptr, host_buf);
    sprintf(err_msg, "Certificate creation request failed for host %s in subchild %d\n", host, sub_child_id);  
    status = run_command(cmd_buffer, err_msg);   
    if(status < 0){
      return NULL;  
    }
    sprintf(cmd_buffer, "openssl x509 -sha256 -req -in /tmp/%s_%d.csr -out /tmp/%s_%d.cer -CAkey %s/cert/proxyCA.key -CA %s/cert/proxyCA.cer -days 365 -CAcreateserial -CAserial serial", host, sub_child_id, host, sub_child_id, ptr, ptr);   
    status = system(cmd_buffer);   
    sprintf(err_msg, "Certificate creation failed for host %s in subchild %d\n", host, sub_child_id);  
    status = run_command(cmd_buffer, err_msg);   
    if(status < 0){
      return NULL;  
    }
         			
    sprintf(cer_file, "/tmp/%s_%d.pem", host, sub_child_id); 
    sprintf(cmd_buffer, "cat %s/cert/proxy.key /tmp/%s_%d.cer >%s", ptr, host, sub_child_id, cer_file);	
    sprintf(err_msg, "Certificate creation failed for host %s in subchild %d\n", host, sub_child_id);  
    status = run_command(cmd_buffer, err_msg);   
    if(status < 0){
      return NULL;  
    }
  }

  //if (!ns_ctx) {
   OpenSSL_add_all_algorithms(); 
   if(proxy_mode !=3){
     //fprintf(stderr, "certificate file name for host %s and subchild %d is %s\n", host, sub_child_id, cer_file);
     ns_ctx=initialize_ctx(cer_file,PASSWORD);
     sprintf(cmd_buffer, "rm /tmp/%s_%d.pem /tmp/%s_%d.csr /tmp/%s_%d.cer", host, sub_child_id, host, sub_child_id, host, sub_child_id); 
     sprintf(err_msg, "Failed in removing Certificate for host %s in subchild %d\n", host, sub_child_id);  
     run_command(cmd_buffer, err_msg);   
   }   
   else     
     ns_ctx=initialize_ctx(buf,PASSWORD);

   load_dh_params(ns_ctx,buf2);

   /* Implementing ECDHE cipher suit because there was an issue observed while recording Michels (app) from Iphone. 
      Iphone was sending only ECDHE cipher suit during client Hello, On the other hand server was unable to select 
      ecdhe cipher suit, resulting to SSL_HANDSHAKE Failure. */

   EC_KEY *ecdh=NULL;
   DH *dh = get_dh2236 ();
   status= SSL_CTX_set_tmp_dh (ns_ctx, dh);

   fprintf(stderr, "SSL_CTX_set_tmp_dh status = [%d]\n", status);
   DH_free (dh);
   ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
   if (ecdh != NULL){
     status = SSL_CTX_set_tmp_ecdh(ns_ctx,ecdh);
     fprintf(stderr, "created curve (NID_X9_62_prime256v1)");
     EC_KEY_free(ecdh);
   } else {
     fprintf(stderr, "unable to create curve (NID_X9_62_prime256v1)");
   }

   fprintf(stderr, "********ECDHE status = [%d]\n", status);

   generate_eph_rsa_key(ns_ctx);
   SSL_CTX_set_session_id_context(ns_ctx,(void*)&s_server_session_id_context,
   							sizeof s_server_session_id_context);
   //}

	    sbio=BIO_new_socket(fd,BIO_NOCLOSE);
            ssl=SSL_new(ns_ctx);
	    printf("ssl=0x%p, sbio=0x%p for sub_child_id = %d\n", ssl, sbio, sub_child_id);
            SSL_set_bio(ssl,sbio,sbio);
      	    printf("Client side ssl set for sub child = %d\n", sub_child_id);
                                                                                
      	    if(((r=SSL_accept(ssl))<=0)){
            //printf("SSL_accept: returned %d sub_child_id = %di\n", r ,sub_child_id);   
              switch (SSL_get_error(ssl, r)) {
	        case SSL_ERROR_NONE:
     		  fprintf(stderr, "SSL_accept: SSL_ERROR_NONE\n");
		  break;
   		case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
		  fprintf(stderr, "SSL_accept error: SSL_ERROR_ZERO_RETURN in SSL_accept for sub_child_id = %d\n", sub_child_id);
                  ERR_print_errors_fp(stderr);
		  return NULL;
      		case SSL_ERROR_WANT_READ:
		  fprintf(stderr, "SSL_aceept error: SSL_ERROR_WANT_READ, in SSL_accept for sub_child_id = %d\n", sub_child_id);
                  ERR_print_errors_fp(stderr);
		  return NULL;
		/* It can but isn't supposed to happen */
      		case SSL_ERROR_WANT_WRITE:
		  fprintf(stderr, "SSL_accept error: SSL_ERROR_WANT_WRITE in SSL_accept for sub_child_id = %d\n", sub_child_id);
                  ERR_print_errors_fp(stderr);
		  return NULL;
  		case SSL_ERROR_SYSCALL: //Some I/O error occurred
       		  fprintf(stderr, "SSL_accept: No more data available, return in SSL_accept for sub_child_id = %d\n", sub_child_id); 
                  ERR_print_errors_fp(stderr);
    		  // return NULL;
   		  if (errno == EINTR)
      		  {
   		    fprintf(stderr, "SSL_read interrupted. Continuing...return in SSL_accept for sub_child_id = %d\n", sub_child_id);
		  }
                  ERR_print_errors_fp(stderr);
	       	  return NULL;
   	        case SSL_ERROR_SSL: //A failure in the SSL library occurred, usually a protocol error
   	          fprintf(stderr, "SSL_accept ERROR SSL_ERROR_SSL ..., fp = %p, return in SSL_accept for sub_child_id = %d\n", ssl, sub_child_id);
      	          ERR_print_errors_fp(stderr);
  	        default:
   		  fprintf(stderr, "SSL_accept ERROR COMMON ... return in SSL_accept for sub_child_id = %d\n", sub_child_id);
                  ERR_print_errors_fp(stderr);
                  return NULL;
	      }
            } 
      	    printf("Client accept done for sub_child_id = %d\n", sub_child_id);
#if 0
     	    if (peek_url(ssl, url)) {
        	printf("peek failed\n");
        	SSL_shutdown(ssl);
        	SSL_free(ssl);
                //close(fd); //this will be close outside.
		return NULL;
            }
#endif
	    return ssl;

}

void *ssl_set_clients(int rfd)
{
  BIO *sbio;
  SSL *ssl;
  SSL_CTX *ctx;
  int status;
  char buf[1024];

  char *ptr = getenv("NS_WDIR");
  if (!ptr) {
    NS_EXIT(1, "NS_WDIR env variable must be defined");
  }

  sprintf (buf, "%s/cert/client_1_1.pem", ptr);

  ctx = initialize_ctx_clients(buf);

  sbio=BIO_new_socket(rfd,BIO_NOCLOSE);
  ssl=SSL_new(ctx);
  SSL_set_bio(ssl,sbio,sbio);
  printf("Server side ssl set\n");
printf("Before %p :\n",ssl);
  status = SSL_connect(ssl);
  printf("After %p :\n",ssl);
  if(status <= 0)
  {
    switch(SSL_get_error(ssl,status))
    {
      case  SSL_ERROR_NONE:
        printf("SSL_ERROR_NONE \n");
        return ssl;
        break;
      case  SSL_ERROR_ZERO_RETURN:
        printf("SSL_ERROR_ZERO_RETURN \n");
        break;
      case  SSL_ERROR_WANT_READ :
        printf(" SSL_ERROR_WANT_READ \n");
        break;
      case  SSL_ERROR_WANT_WRITE :
        printf(" SSL_ERROR_WANT_WRITE \n");
        break;
      case  SSL_ERROR_WANT_CONNECT:
        printf("SSL_ERROR_WANT_CONNECT\n");
        break;
      case  SSL_ERROR_WANT_ACCEPT:
        printf("SSL_ERROR_WANT_ACCEPT\n");
        break;
      case  SSL_ERROR_WANT_X509_LOOKUP :
        printf("SSL_ERROR_WANT_X509_LOOKUP \n");
        break;
      case  SSL_ERROR_SYSCALL :
        printf(" SSL_ERROR_SYSCALL \n");
        break;
      case  SSL_ERROR_SSL:
        printf(" SSL_ERROR_SSL\n");
        break;
      default:
        printf("Unknown \n");
        break;
    }
    berr_exit("SSL connect error");
  }
  printf("Server connect set\n");
  return ssl;
}
                               


void *
ssl_set_remote (int rfd)
{
BIO *sbio;
SSL *ssl;
int status;
char buf[1024];
char buf2[1024];

  if (!ns_ctx) {
    char *ptr = getenv("NS_WDIR");
    if (!ptr) {
        NS_EXIT(1, "NS_WDIR env variable must be defined");
    }

    sprintf (buf, "%s/cert/client_cert.pem", ptr);
    sprintf (buf2, "%s/cert/%s", ptr, DHFILE);
    
    ns_ctx=initialize_ctx_clients(buf);
    load_dh_params(ns_ctx,buf2);
    generate_eph_rsa_key(ns_ctx);
    SSL_CTX_set_session_id_context(ns_ctx,(void*)&s_server_session_id_context, sizeof s_server_session_id_context);
  }

      sbio=BIO_new_socket(rfd,BIO_NOCLOSE);
      ssl=SSL_new(ns_ctx);
      SSL_set_bio(ssl,sbio,sbio);
      printf("Server side ssl set\n");
	  printf("Before %p :\n",ssl);
	 status = SSL_connect(ssl);   
	  printf("After %p :\n",ssl);
      if(status<=0)
	 {
	   switch(SSL_get_error(ssl,status))
	   {
			case  SSL_ERROR_NONE:
				printf("SSL_ERROR_NONE \n");
			return ssl;
			break;
			case  SSL_ERROR_ZERO_RETURN:
				printf("SSL_ERROR_ZERO_RETURN \n");
			break;
			case  SSL_ERROR_WANT_READ :
				printf(" SSL_ERROR_WANT_READ \n");
			break;
			case  SSL_ERROR_WANT_WRITE :
				printf(" SSL_ERROR_WANT_WRITE \n");
			break;
			case  SSL_ERROR_WANT_CONNECT:
				printf("SSL_ERROR_WANT_CONNECT\n");
			break;
			case SSL_ERROR_WANT_ACCEPT:
				printf("SSL_ERROR_WANT_ACCEPT\n");
			break;
			case SSL_ERROR_WANT_X509_LOOKUP :
				printf("SSL_ERROR_WANT_X509_LOOKUP \n");
			break;
			case  SSL_ERROR_SYSCALL :
				printf(" SSL_ERROR_SYSCALL \n");
			break;
			case   SSL_ERROR_SSL:
				printf(" SSL_ERROR_SSL\n");
			break;
			
			default:
				printf("Unknown \n");
			break;
	   }
 	
   
		
		berr_exit("SSL connect error");

	 }
                                                                                
      printf("Server connet set\n");

	return ssl;
}

void
ssl_end()
{
	if (ns_ctx)
	destroy_ctx(ns_ctx);
}

//<amitabh>
/*	int proxy_chain(int remote_server_fd, char *cur_host_name)
{
 	char buffer[512];
	char buf[1024] ;
	char *ptr ;
buffer = (char *)malloc(sizeof(char) * (strlen ("CONNECT ") + strlen(cur_host_name) + strlen("HTTP/1.1") + 4));
	if(buffer == NULL)
	{
		printf("proxy_chain: malloc on buffer failed\n");
		return -1;
	}

	sprintf(buffer,"CONNECT %s  HTTP/1.1\r\nHost: %s\r\n\r\n",cur_host_name, cur_host_name);
 	printf("Sending %s\n",buffer);
    writestr(remote_server_fd,buffer);
 	//free(buffer);
	printf("Send success\n");
	if (recv(remote_server_fd, buf, 1024 ,0) <= 0) 
	{
      printf("proxy_chain:Empty  response");
      return (-1);
    }

	else
	{
		printf("Recv buffer = <%s>",buf);
		ptr = 	strtok(buffer," ");
		ptr = 	strtok(NULL," ");
		if(strcmp(ptr,"200"))
		{
			printf("buf recv %s\n",buf);
		}
		else
		{
			printf("proxy_chain : readline:recv buffer %s\n",buf);
			return -1;
		}

	}
	 return 1;
}
//</amitabh> */
/*int proxy_chain(int remote_server_fd, char *cur_host_name)
{
 	char buffer[4096];
	char buf[1024] ;
	char *ptr ;
	buffer = (char *)malloc(sizeof(char) * (strlen ("CONNECT ") + strlen(cur_host_name) + strlen("HTTP/1.1") + 4));
	if(buffer == NULL)
	{
		printf("proxy_chain: malloc on buffer failed\n");
		return -1;
	}
 
	sprintf(buffer,"CONNECT %s HTTP/1.1\r\nHost: %s\r\n\r\n",cur_host_name, cur_host_name);
 	printf("Sending %s\n",buffer);
    	writestr(remote_server_fd,buffer);
 	//free(buffer);
	printf("Send success\n");


	if (readline(remote_server_fd, buf, 1024) <= 0) {
      	    printf("proxy_chain:Empty  response");
      	    return (-1);
    	} else {
		ptr = 	strtok(buffer," ");
		ptr = 	strtok(NULL," ");
		if(strcmp(ptr,"200"))
		{
			printf("buf recv %s\n",buf);
		}
		else
		{
			printf("proxy_chain : readline:recv buffer %s\n",buf);
			return -1;
		}

	}
     //Look for the blank line that signals end of header
    while(readline(remote_server_fd,buf,sizeof(buf))>0) {
      ;
    }
	 return 1;
}*/
//</amitabh>

//<amitabh>

/*
	called when proxychaning is enabled. Used to sedn CONNECT to existing proxy
*/
int proxy_chain(int remote_server_fd, char *cur_host_name)
{
 	char buffer[4096];
	char buf[1024] ;
	char *ptr ;
 	int is_port = 0;

	if (index(cur_host_name, ':')) 
		is_port = 1;
 
	sprintf(buffer,"CONNECT %s%s HTTP/1.1\r\nHost: %s\r\n\r\n",cur_host_name, is_port?"":":443", cur_host_name);
 	printf("proxy_chain Sending  %s\n",buffer);
    writestr(remote_server_fd,buffer);
	printf("proxy_chain Send success\n");
	if (readline(remote_server_fd, buf, 1024) <= 0)
	{
      	    printf("proxy_chain:Empty  response");
      	    return (-1);
    }
	else 
	{
		ptr = 	strtok(buf," ");
		ptr = 	strtok(NULL," ");
		if(!strcmp(ptr,"200"))
		{
			printf("buf recv %s\n",buf);
		}
		else
		{
			printf("proxy_chain : readline:recv buffer %s\n",buf);
			return -1;
		}

	}
    /* Look for the blank line that signals end of header*/
    while(readline(remote_server_fd,buf,sizeof(buf))>0) {
      ;
    }
	 return 1;
}
//</amitabh>
