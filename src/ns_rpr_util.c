
//gcc -fgnu89-inline -I./ -I/home/anil/WORK/cavisson/base/include/libnscore -I./../../base/libnscore/ -I./../../base/libnscore//.. -I./../../netstorm/src/ -I/home/anil/WORK/cavisson/base/include/openssl/openssl-1.0.2h/include -I/home/anil/WORK/cavisson/base/include/xml2 -I/usr/include/postgresql/ -I/home/anil/WORK/cavisson/base/include/memcached/libmemcached-1.0.18/ -I-I/home/anil/WORK/cavisson/base/include/cprops -I/home/anil/WORK/cavisson/base/include/jms -I/home/anil/WORK/cavisson/base/include/http2 -I/home/anil/WORK/cavisson/base/include/brotli/c -I/home/anil/WORK/cavisson/base/include/brotli/c/include -I/home/anil/WORK/cavisson/base/include/cprops -I/home/anil/WORK/cavisson/base/include/ntlm/libntlm-1.3 -I/usr/include/x86_64-linux-gnu/ -Wall -g -m64 -fgnu89-inline -c -DENABLE_SSL -DNS_TIME -DNS_USE_MODEM -DUSE_EPOLL -DUbuntu -DRELEASE=1604 -DENABLE_WAN_EX -DENABLE_JIFFY_TS -DPG_BULKLOAD -D_GNU_SOURCE -o /home/anil/WORK/cavisson/prod-src/core/netstorm/src/build/obj/ns_rpr_util.o ns_rpr_util.c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>


#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/dh.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/err.h>

#include "tmr.h"
#include "ns_rpr.h"
#include "ns_rpr_util.h"
#include "nslb_ssl_lib.h"
//#include "ns_hash_util.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  #include <openssl/ssl_cav.h>
#endif

// All files with relative path are taken from $HPD_ROOT/cert
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  #define DEFAULT_SERVER_CERT_FILE "cav-test-server.pem"
#else
  #define DEFAULT_SERVER_CERT_FILE "cav-test-server.pem"
#endif

char rpr_conf_dir[MAX_FILE_NAME_LEN];
char g_rpr_logs_dir[MAX_FILE_NAME_LEN];

int debug_log = 0;
int error_fd = -1;
int debug_fd = -1;
unsigned int max_error_log_file_size = DEFAULT_MAX_ERROR_FILE_SIZE;  // Approx 10 MB
unsigned int max_debug_log_file_size = DEFAULT_MAX_DEBUG_FILE_SIZE;  // Approx 100MB

static int verify_depth = 1;
static char server_cert_file[2048 + 1];
static char ca_list[2048 + 1];
static char dh_file[2048 + 1];
static char random_file[2048 + 1];
static char crl_file[2048 + 1];
static char ssl_ciphers[2048 +1]={0};  //store cipher
static char tls_version = 0;
static int num_extra_cert_file = 0;
static char server_extra_cert_file[MAX_FILE][MAX_FILE_LEN];
static char g_ssl_cert_pass[512] = "password";
static int g_ssl_client_authentication = 0;
static int ssl_regenotiation = 0;

static int s_server_session_id_context = 1;
SSL_CTX *my_ctx;

//static BIO *bio_err=0;
//static BIO *bio_err=0;
static char *pass;
//static char *pass;

void rpr_write_all(int fd, char *buf, int size) {
  int written=0;
  int len;

  //NSDL2_CLASSIC_MON("Method called, fd = %d, size = %d, buf = [%s]", fd, size, buf);
  RPR_DL(NULL, "Method called, fd = %d, size = %d", fd, size);
  if(fd < 0) return;  // Why call this if -ve, check later

  while (written < size) {
    len = write (fd, buf+written, size - written);
    if (len < 0 ) {
      //perror("write failed while dumping data");
      break;
    }
    written += len;
  }
  close (fd);
}

char *rpr_get_cur_date_time()
{
  time_t  tloc;
  struct  tm *lt;
  static  char cur_date_time[100];
  sigset_t sigset, oldset;

  (void)time(&tloc);
  /*In case of HPD recovery, while running hpd with debug it uses localtime function which acquire mutux lock 
    to compute time.Now if sick child is receive then signal the handler also calls localtime(), 
    it causes localtime to take the mutex. The mutex is already locked by the same thread. Hence causes Deadlock. 
    Solution: Block all signals while getting current date time stamp 
  */
  sigemptyset(&sigset);
  sigfillset(&sigset);
  sigprocmask(SIG_BLOCK, &sigset, &oldset);
  lt = localtime(&tloc);
  sigprocmask(SIG_UNBLOCK, &sigset, &oldset);

  if(lt  == (struct tm *)NULL)
    strcpy(cur_date_time, "Error");
  else
    sprintf(cur_date_time, "%02d/%02d/%02d %02d:%02d:%02d", 1900 + lt->tm_year, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_date_time);
}

void open_log(char *name, int *fd, unsigned int max_size, char *header)
{
  char log_file[1024], log_file_prev[1024];
  struct stat stat_buf;
  int status;

  //sprintf(log_file, "%s/%s", rpr_conf_dir, name);
  sprintf(log_file, "%s/%s", g_rpr_logs_dir, name);
  //printf("In side %s for file %s\n", (char *)__FUNCTION__, log_file);
  // get debug or error log file size using stat and check this size > max size of debug or error log file
  if((stat(log_file, &stat_buf) == 0) && (stat_buf.st_size > max_size))
  {
   // check if fd is open, close it
    if(*fd > 0)
    {
      close(*fd);
      *fd = -1;
    }
    sprintf(log_file_prev, "%s.prev", log_file);

    // Never use debug_log from here
    if (debug_log) printf("Moving file %s with size %lu to %s, Max size = %u\n", log_file, stat_buf.st_size, log_file_prev, max_size);
   status = rename(log_file, log_file_prev);
   if(status < 0)
    // Never use debug_log from here
     fprintf(stderr, "Error in moving '%s' file, err = %s\n", log_file, strerror(errno));
  }

  if (*fd < 0 ) //if fd is not open then open it
  {
    *fd = open (log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
    if (*fd < 0)
    {
      fprintf(stderr, "Error: Error in opening file '%s', fd = %d, Error = '%s'\n", log_file, *fd, strerror(errno));
      exit (-1);
    }
    if(header)
    {
      write(debug_fd, DEBUG_HEADER, strlen(DEBUG_HEADER));
      write(error_fd, DEBUG_HEADER, strlen(DEBUG_HEADER));
    }
  }
}

void rpr_error_log_ex(char *file, int line, char *fname, void *cptr_void, char *format, ...)
{
  va_list ap;
  char buffer[MAX_LOG_BUF_SIZE + 1];
  int amt_written1 = 0, amt_written = 0;
  char log_file[1024];
  connection *cptr = cptr_void;

  //if(log_level > g_err_log) return;

  if(cptr != NULL) 
    amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|%d|%d|%u|%d|", rpr_get_cur_date_time(), file, line, fname, g_rpr_id, getpid(), cptr->fd, cptr->state);
  else
    amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|%d|%d|NA|NA|NA|", rpr_get_cur_date_time(), file, line, fname, g_rpr_id, getpid());

  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  buffer[MAX_LOG_BUF_SIZE] = 0;

  // In some cases, vsnprintf return -1 but data is copied in buffer
  if(amt_written < 0)
  {
    amt_written = strlen(buffer) - amt_written1;
  }

  if(amt_written > (MAX_LOG_BUF_SIZE - amt_written1))
  {
    amt_written = (MAX_LOG_BUF_SIZE - amt_written1);
  }

  //TODO: change the path of error log file to capture directory
  sprintf(log_file, "rpr_error.%d.log", g_rpr_id);
  open_log(log_file, &error_fd, max_error_log_file_size, DEBUG_HEADER);
  write(error_fd, buffer, amt_written + amt_written1);
}

void rpr_exit(char *file, int line, char *fname, int exit_status, char *format, ...)
{ 
  char exit_status_file_name[ERROR_BUF_SIZE/4];
  FILE *exit_file_fp;
  va_list ap;
  char buffer[ERROR_BUF_SIZE + 1];
  int amt_written1, amt_written;
  char *ptr;
  char rpr_conf_dir[1024] = "";
  
  amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|", rpr_get_cur_date_time(), file, line, fname);
  
  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, ERROR_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);
  
  //TODO: change the path of exit status file to capture directory
  ptr = getenv("NS_WDIR");
  if(ptr)
    strcpy(rpr_conf_dir, ptr);
  else
  { 
    RPR_DL(NULL, "NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work");
    strcpy(rpr_conf_dir, "/home/cavisson/work");
  }
  
  buffer[amt_written + amt_written1] = '\0';
  sprintf(exit_status_file_name, "%s/rpr_exit_status.log", g_rpr_logs_dir);
  exit_file_fp = fopen(exit_status_file_name, "w");
  
  fprintf(exit_file_fp, "%s\n", buffer);
  fprintf(stderr, "%s\n", buffer); 
  rpr_error_log(file, line, fname, NULL, "%s", buffer + amt_written1);
  
  fclose(exit_file_fp);
  exit(exit_status);
}

void rpr_debug_log_ex(char *file, int line, char *fname, void *cptr_void, char *format, ...)
{
  va_list ap;
  char buffer[MAX_LOG_BUF_SIZE + 1];
  int amt_written1 = 0, amt_written = 0;
  char log_file[1024];

  connection *cptr = cptr_void;

  if(debug_log == 0)
    return;

  if(cptr != NULL)
    amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|%d|%d|%u|%d|%d|", rpr_get_cur_date_time(), file, line, fname, g_rpr_id, getpid(), cptr->fd, cptr->state, cptr->type);
  else
    amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|%d|%d|NA|NA|NA|", rpr_get_cur_date_time(), file, line, fname, g_rpr_id, getpid());

  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  buffer[MAX_LOG_BUF_SIZE] = 0;

  // In some cases, vsnprintf return -1 but data is copied in buffer
  if(amt_written < 0)
  {
    amt_written = strlen(buffer) - amt_written1;
  }

  if(amt_written > (MAX_LOG_BUF_SIZE - amt_written1))
  {
    amt_written = (MAX_LOG_BUF_SIZE - amt_written1);
  }

  sprintf(log_file, "rpr_debug.%d.log", g_rpr_id);
  open_log(log_file, &debug_fd, max_debug_log_file_size, DEBUG_HEADER);
  write(debug_fd, buffer, amt_written + amt_written1);
}

//This method is to set default certificates and crl file
void init_ssl_default(char *g_wdir)
{
  RPR_DL(NULL, "Method called. g_wdir = %s", g_wdir);
  verify_depth = 1; 
  sprintf(server_cert_file, "%s/cert/%s", g_wdir, DEFAULT_SERVER_CERT_FILE);
  sprintf(ca_list, "%s/cert/%s", g_wdir, DEFAULT_CA_LIST);
  sprintf(dh_file, "%s/cert/%s", g_wdir, DEFAULT_DH_FILE);
  sprintf(random_file, "%s/cert/%s", g_wdir, DEFAULT_RANDOM_FILE);
  strcpy(crl_file, ""); //default CRL disable 
  strcpy(ssl_ciphers, ""); //default cipher disable
}

#if OPENSSL_VERSION_NUMBER >= 0x10100000L 
static DH *get_dh512(void)
{   
    static unsigned char dh512_p[] = {
        0xCB, 0xC8, 0xE1, 0x86, 0xD0, 0x1F, 0x94, 0x17, 0xA6, 0x99, 0xF0,
        0xC6, 
        0x1F, 0x0D, 0xAC, 0xB6, 0x25, 0x3E, 0x06, 0x39, 0xCA, 0x72, 0x04,
        0xB0, 
        0x6E, 0xDA, 0xC0, 0x61, 0xE6, 0x7A, 0x77, 0x25, 0xE8, 0x3B, 0xB9,
        0x5F, 
        0x9A, 0xB6, 0xB5, 0xFE, 0x99, 0x0B, 0xA1, 0x93, 0x4E, 0x35, 0x33,
        0xB8, 
        0xE1, 0xF1, 0x13, 0x4F, 0x59, 0x1A, 0xD2, 0x57, 0xC0, 0x26, 0x21,
        0x33, 
        0x02, 0xC5, 0xAE, 0x23,
    };
    static unsigned char dh512_g[] = {
        0x02,
    }; 
    DH *dh;
    BIGNUM *p, *g;

    if ((dh = DH_new()) == NULL)
        return NULL;
    p = BN_bin2bn(dh512_p, sizeof(dh512_p), NULL);
    g = BN_bin2bn(dh512_g, sizeof(dh512_g), NULL);
    if ((p == NULL) || (g == NULL) || !DH_set0_pqg(dh, p, NULL, g)) {
        DH_free(dh);
        BN_free(p);
        BN_free(g);
        return NULL;
    }
    return dh;
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L 
static DH *get_dh512()
        {
        static unsigned char dh512_p[]={
                0xCB,0xC8,0xE1,0x86,0xD0,0x1F,0x94,0x17,0xA6,0x99,0xF0,0xC6,
                0x1F,0x0D,0xAC,0xB6,0x25,0x3E,0x06,0x39,0xCA,0x72,0x04,0xB0,
                0x6E,0xDA,0xC0,0x61,0xE6,0x7A,0x77,0x25,0xE8,0x3B,0xB9,0x5F,
                0x9A,0xB6,0xB5,0xFE,0x99,0x0B,0xA1,0x93,0x4E,0x35,0x33,0xB8,
                0xE1,0xF1,0x13,0x4F,0x59,0x1A,0xD2,0x57,0xC0,0x26,0x21,0x33,
                0x02,0xC5,0xAE,0x23,
                };
        static unsigned char dh512_g[]={
                0x02,
                };
        DH *dh;

        if ((dh=DH_new()) == NULL) return(NULL);
        dh->p=BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
        dh->g=BN_bin2bn(dh512_g,sizeof(dh512_g),NULL);
        if ((dh->p == NULL) || (dh->g == NULL))
                { DH_free(dh); return(NULL); }
        return(dh);
        }
#endif

/*The password code is not thread safe*/
static int password_cb(char *buf,int num,int rwflag,void *userdata)
{
  if(num < strlen(pass) + 1)
    return(0);

  strcpy(buf, pass);
  return(strlen(pass));
}

static void load_dh_params(ctx,file)
  SSL_CTX *ctx;
  char *file;
  {
    DH *ret=0;
    BIO *bio;

    RPR_DL(NULL, "Method called. file name = %s", file);

    if ((bio=BIO_new_file(file,"r")) == NULL)
      rpr_berr_exit("Couldn't open DH file");

    ret=PEM_read_bio_DHparams(bio,NULL,NULL,NULL);
    BIO_free(bio);
    if(SSL_CTX_set_tmp_dh(ctx,ret)<0)
      rpr_berr_exit("Couldn't set DH parameters");
  }

static void generate_eph_rsa_key(ctx)
  SSL_CTX *ctx;
  {
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
      RSA *rsa;

      rsa=RSA_generate_key(512,RSA_F4,NULL,NULL);

      if (!SSL_CTX_set_tmp_rsa(ctx,rsa))
       rpr_berr_exit("Couldn't set RSA key");

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

static SSL_CTX *initialize_ctx_local(char *keyfile, char *password)
{
  SSL_METHOD *meth = 0;
  SSL_CTX *ctx = 0;
  X509 *x_ptr;
  //X509 *cert_ptr;
  int i;
  int ret;
  int rc;
  FILE *fp = NULL;
  char err[1024] = "";

  RPR_DL(NULL, "Method called.");

  if(!bio_err)
  {
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
    /* Global system initialization*/
    SSL_library_init();
    SSL_load_error_strings();
    #endif

    /* An error write context */
    bio_err=BIO_new_fp(stderr, BIO_NOCLOSE);
  }


   #if OPENSSL_VERSION_NUMBER >= 0x10100000L
   ctx = SSL_CTX_new(TLS_method());
   switch (tls_version)
   {
     case SSL3_0:
          RPR_DL(NULL,  "Method called SSLv3_client_method");
          SSL_CTX_set_min_proto_version(ctx, SSL3_VERSION);
          SSL_CTX_set_max_proto_version(ctx, SSL3_VERSION);
          break;
     case TLS1_0:
          RPR_DL(NULL,  "Method called TLSv1.0_client_method");
          SSL_CTX_set_min_proto_version(ctx, TLS1_VERSION);
          SSL_CTX_set_max_proto_version(ctx, TLS1_VERSION);
          break;
     case TLS1_1:
          RPR_DL(NULL,  "Method called TLSv1.1_client_method");
          SSL_CTX_set_min_proto_version(ctx, TLS1_1_VERSION);
          SSL_CTX_set_max_proto_version(ctx, TLS1_1_VERSION);
          break;
     case TLS1_2:
          RPR_DL(NULL,  "Method called TLSv1.2_client_method");
          SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
          SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION);
          break;
     case TLS1_3:
          RPR_DL(NULL,  "Method called TLSv1.3_client_method");
          SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
          SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
          break;
     default:
          RPR_DL(NULL,  "Method called TLS_client_method");
   }
   #else
  /* Create our context*/
  switch (tls_version)
  {
    case SSL3_0:
      RPR_DL(NULL,"Method called SSLv3_server_method");
      if (!(meth = (SSL_METHOD *)SSLv3_server_method ()))
        rpr_berr_exit("initialize_ctx: SSLv3_server_method() failed");
      break;
    case TLS1_0:
      RPR_DL(NULL, "Method called TLSv1_server_method");
      if (!(meth = (SSL_METHOD *)TLSv1_server_method ()))
        rpr_berr_exit("initialize_ctx: TLSv1_server_method() failed");
      break;
    case TLS1_1:
      RPR_DL(NULL, "Method called TLSv1_1_server_method");
      if (!(meth = (SSL_METHOD *)TLSv1_1_server_method ()))
        rpr_berr_exit("initialize_ctx:TLSv1_1_server_method () failed");
      break;
    case TLS1_2:
      RPR_DL(NULL,"Method called TLSv1_2_server_method");
      if (!(meth = (SSL_METHOD *)TLSv1_2_server_method ()))
        rpr_berr_exit("initialize_ctx:TLSv1_2_server_method () failed");
     break;
    default:
      RPR_DL(NULL,"Method called SSLv23_server_method");
      if (!(meth = (SSL_METHOD *)SSLv23_server_method ()))
        rpr_berr_exit("initialize_ctx: SSLv23_server_method() failed");
  }
  if (!(ctx = SSL_CTX_new (meth)))
    rpr_berr_exit("initialize_ctx: Can't create SSL_CTX object.");
  #endif


  //BUG#20188: For ssl version 1.0.2h it is requied. For large data file SSL_write does not work properly with it.
  SSL_CTX_set_mode(ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  /* Load our keys and certificates*/
  RPR_DL(NULL, "Loading Certificate from %s into ctx", keyfile);

  /* Load randomness */
  pass=password;
  RPR_DL(NULL, "Setting Password '%s' into ctx", pass);
  SSL_CTX_set_default_passwd_cb(ctx, password_cb);

  /* SSL_CTX_use_certificate_chain_file() only works on PEM files */
  //if(!(SSL_CTX_use_certificate_chain_file(ctx, keyfile)))
  /* Enable the use of certificate chains */
  rc = SSL_CTX_use_certificate_chain_file(ctx, keyfile);

  switch (rc)
  {
    case 1:
      RPR_DL(NULL, "SSL certificate (%s) load ok\n", keyfile);
      break;
    default:
      fprintf(stderr, "SSL cert load error [%s]\n", ERR_error_string(ERR_get_error(), NULL));
      sprintf(err, "Cannot load certificate chain file '%s'", keyfile);
      rpr_berr_exit(err);
  }


  // This code is added to load the complete chain of certificates
  // SSLExtraCertificateChainFile is used to add extra files that complete chain
  // d2i_X509_fp api is used to read certificate and encode
  // SSL_CTX_add_extra_chain_cert add the certificate into chain  
  for(i = 0; i < num_extra_cert_file; i++ )
  {
    RPR_DL(NULL, "server_extra_cert_file = %s, i = %d\n", server_extra_cert_file[i], i);
    fp = fopen(server_extra_cert_file[i], "rb");

    if(fp == NULL)
      RPR_EXIT(-1, _FLN_, "Error in opening chain file [%s]\n", server_extra_cert_file[i]);

    x_ptr = d2i_X509_fp(fp, NULL);

    if(x_ptr == NULL)
      RPR_EXIT(-1, _FLN_, "Error in d2i_X509_fp file [%s], Error = [%s]\n", server_extra_cert_file[i], ERR_error_string(ERR_get_error(), NULL));

    ret = SSL_CTX_add_extra_chain_cert(ctx, x_ptr);
    RPR_DL(NULL, "value of ret[%d] = [%d]", i, ret);
    if(ret != 1){
      fprintf(stderr, "Error in  SSL_CTX_add_extra_chain_cert file [%s], Error = [%s]\n", server_extra_cert_file[i], ERR_error_string(ERR_get_error(), NULL));
    }
    fclose(fp);
  }

  //comment this since test with chain file
  //if(!(SSL_CTX_use_certificate_file(ctx,keyfile,SSL_FILETYPE_PEM)))
    //rpr_berr_exit("Couldn't read certificate file");

  RPR_DL(NULL, "Loading Private key '%s' into ctx", keyfile);
  rc =SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM);
  switch (rc)
  {
    case 1:
      RPR_DL(NULL, "SSL private key load ok\n");
      break;
    default:
      fprintf(stderr, "SSL private key load error [%s]\n", ERR_error_string(ERR_get_error(), NULL));
      sprintf(err, "Cannot load private key file '%s'", keyfile);
      rpr_berr_exit(err);
  }

  /* Check Private Key */
  if (!SSL_CTX_check_private_key(ctx))
    rpr_berr_exit("Private key does not match the certificate");
  /* Load CA file for verifying peer supplied certificate */
  if(!(SSL_CTX_load_verify_locations(ctx, ca_list, 0)))
  {
    sprintf(err, "Couldn't read root CA file '%s'", ca_list);
    rpr_berr_exit(err);
  }

  if(!(RAND_load_file(random_file, 1024*1024)))
  {
    sprintf(err, "Couldn't load random file '%s'", random_file);
    rpr_berr_exit(err);
  }

  if (strcasecmp(crl_file, "") != 0)
  {
    //A CRL is a file, created by the certificate issuer that lists all the certificates that it previously signed, but which it now revokes. CRLs are in PEM format.
    //If the peer certificate does not pass the revocation list, or if no CRL is found, then the handshaking fails with an error.
    X509_STORE_set_flags(SSL_CTX_get_cert_store(ctx), X509_V_FLAG_CRL_CHECK |X509_V_FLAG_CRL_CHECK_ALL);
    //After setting this flag, if OpenSSL checks a peer's certificate, then it will attempt to find a CRL for the issuer.
    if (X509_load_crl_file(X509_STORE_add_lookup(SSL_CTX_get_cert_store(ctx), X509_LOOKUP_file()), crl_file, X509_FILETYPE_PEM) != 1)
    {
      sprintf(err, "Couldn't load CRL file '%s'", crl_file);
      rpr_berr_exit(err);
    }
    else
      RPR_DL(NULL, "CRL file '%s' load ok", crl_file);
  }

  return ctx;
}

void init_ssl()
{
  RPR_DL(NULL, "Method called.");
  char ciphers[2048] = "";
  EC_KEY *ecdh=NULL;
  int status;
  char error[512];

  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  /* Build our SSL context*/
  my_ctx=initialize_ctx_local(server_cert_file, g_ssl_cert_pass);
  load_dh_params(my_ctx,dh_file);
  //Following line Added for TST
  //ciphers[0] = 0;

  // Here we are restricting all ciphers that use any of following algorathims: RC4, 3DES, DES
  // These ciphers are weak ciphers and we got requirement from wellsfargo Qualys scan to disbale these ciphers. By following change these 
  // ciphers will be disabled by default, but someone can specify these in cipher keywords ans use these for testing  


  //previously strcat was used, which adds space initially. 

  #if OPENSSL_VERSION_NUMBER >= 0x10100000L
     if (tls_version == TLS1_3)
       strcpy(ciphers, "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256");
     else
       strcpy(ciphers, ":ALL:!RC4:!3DES:!DES");
  #else
     strcpy(ciphers, ":ALL:!RC4:!3DES:!DES");
  #endif

  // If they want the null ones, add them in
  // strcat(ciphers,":NULL");

  // If they want the anonymous ones, add them in
  // strcat(ciphers,":aNULL");

  if(ssl_ciphers[0]){
     if (tls_version == TLS1_3)
     {
       #if OPENSSL_VERSION_NUMBER >= 0x10100000L
       status = SSL_CTX_set_ciphersuites(my_ctx, ssl_ciphers);
       #endif
     }
     else
       status = SSL_CTX_set_cipher_list(my_ctx, ssl_ciphers);
  }else{
     if (tls_version == TLS1_3)
     {
       #if OPENSSL_VERSION_NUMBER >= 0x10100000L
       status = SSL_CTX_set_ciphersuites(my_ctx, ciphers);
       #endif
     }
     else
       status = SSL_CTX_set_cipher_list(my_ctx, ciphers);
  }

  //status = SSL_CTX_set_cipher_list(my_ctx, "ECDHE-ECDSA-AES128-SHA256");

  RPR_DL(NULL, "status = [%d]", status);
  if(!status)
  {
    RPR_EXIT(-1, _FLN_, "Cipher suits failed to add, error = %s", ERR_error_string(ERR_get_error(), error));
    RPR_DL(NULL, "Cipher suits failed to add, error = %s", ERR_error_string(ERR_get_error(), error));
  }

  status = SSL_CTX_set_tmp_dh(my_ctx,get_dh512());
  // status = SSL_CTX_set_tmp_dh(my_ctx,get_dh1024());
  RPR_DL(NULL, "SSL_CTX_set_tmp_dh status = [%d]", status);

  #if OPENSSL_VERSION_NUMBER >= 0x10100000L
    ecdh = EC_KEY_new_by_curve_name(NID_X25519);
  #else
    ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  #endif

 // ecdh = EC_KEY_new_by_curve_name(NID_secp384r1);
  if (ecdh == NULL)
    RPR_DL(NULL, "unable to create curve (nistp256)");

  status = SSL_CTX_set_tmp_ecdh(my_ctx,ecdh);
  RPR_DL(NULL, "********ECDHE status = [%d]", status);
  EC_KEY_free(ecdh);

  generate_eph_rsa_key(my_ctx);

  if( !SSL_CTX_set_session_id_context(my_ctx,(void*)&s_server_session_id_context, sizeof s_server_session_id_context))
    rpr_berr_exit("SSL_CTX_set_session_id_context failed");
}

/*
static inline void set_ssl_client_auth(SSL_CTX *ctx)
{
  RPR_DL(NULL, "Method called.");

  SSL_CTX_set_verify(ctx, hpd_global_config->g_ssl_client_authentication, ssl_verify_callback_for_client);
  SSL_CTX_set_default_verify_paths(ctx);
  SSL_CTX_set_verify_depth(ctx, verify_depth);
  RPR_DL(NULL, "Set maximum depth for the certificate chain verification is %d", verify_depth);
}
*/

/* Print SSL errors and exit*/
void rpr_berr_exit(char *string)
{
  BIO_printf(bio_err,"%s\n",string);
  ERR_print_errors(bio_err);
  RPR_EXIT(0, _FLN_, "SSL error");
}

void *ssl_set_clients_local(connection *cptr, int rfd, char *host_name, char *buf)
{
  RPR_DL(NULL, "Method called.");
  BIO *sbio;
  SSL *ssl;
  SSL_CTX *ctx;
  int status;
  char host_buf[1024] = {0};

  char *ptr = getenv("NS_WDIR");
  if (!ptr) {
    RPR_DL(NULL, "NS_WDIR env variable must be defined");
  }

  ctx = initialize_ctx_clients(buf);

  sbio=BIO_new_socket(rfd,BIO_NOCLOSE);
  ssl=SSL_new(ctx);
  SSL_set_bio(ssl,sbio,sbio);
  RPR_DL(NULL, "Server side ssl set\n");
  RPR_DL(NULL, "Before %p :\n",ssl);

  strcpy(host_buf, host_name);

  char *ptr_del = strchr(host_buf, ':');
  if(ptr_del)
  {
    *ptr_del = '\0';
  }

  if (!SSL_set_tlsext_host_name(ssl, host_buf)) {
    fprintf(stderr, "Unable to send SNI for hostname = %s", host_buf);
    return NULL;
  }

  status = SSL_connect(ssl);
  RPR_DL(NULL, "After %p :\n",ssl);
  if(status <= 0)
  {
    switch(SSL_get_error(ssl,status))
    {
      case  SSL_ERROR_NONE:
        RPR_DL(NULL, "SSL_ERROR_NONE \n");
        return ssl;
        break;
      case  SSL_ERROR_ZERO_RETURN:
        RPR_DL(NULL, "SSL_ERROR_ZERO_RETURN \n");
        break;
      case  SSL_ERROR_WANT_READ :
        RPR_DL(NULL, "SSL_ERROR_WANT_READ \n");
        cptr->state = CNST_SSLCONNECTING;
	break;
      case  SSL_ERROR_WANT_WRITE :
        RPR_DL(NULL, "SSL_ERROR_WANT_WRITE \n");
        break;
      case  SSL_ERROR_WANT_CONNECT:
        RPR_DL(NULL, "SSL_ERROR_WANT_CONNECT\n");
        cptr->state = CNST_SSLCONNECTING;
        break;
      case  SSL_ERROR_WANT_ACCEPT:
        RPR_DL(NULL, "SSL_ERROR_WANT_ACCEPT\n");
        break;
      case  SSL_ERROR_WANT_X509_LOOKUP :
        RPR_DL(NULL, "SSL_ERROR_WANT_X509_LOOKUP \n");
        break;
      case  SSL_ERROR_SYSCALL :
        RPR_DL(NULL, "SSL_ERROR_SYSCALL \n");
        break;
      case  SSL_ERROR_SSL:
        RPR_DL(NULL, "SSL_ERROR_SSL\n");
        break;
      default:
        RPR_DL(NULL, "Unknown \n");
        break;
    }
    rpr_berr_exit("SSL connect error");
  }
  RPR_DL(NULL, "Server connect set\n");
  return ssl;
}




int rpr_datawrite(char * buf, int size, void *dest, int dest_type)
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
    return INVALID_TYPE;

  return SUCCESS;
}

/* now we check to see which server we talked to */
static inline void check_ssl_client_cert(connection *cptr)
{
  X509 *cert;
  long verify_result;
  RPR_DL(cptr, "Method called. To check client certificate");

  cert = SSL_get_peer_certificate(cptr->ssl);
  if(cert != NULL)
  {
    verify_result = SSL_get_verify_result(cptr->ssl);
    RPR_DL(cptr, "SSL_get_verify_result '%s'", X509_verify_cert_error_string(verify_result));
    if( verify_result != X509_V_OK )
    {
      RPR_DL(cptr, "SSL_connect failed, because client certificat not verified: '%s'", X509_verify_cert_error_string(verify_result));
    X509_free(cert);
    }
    else if (SSL_get_verify_result(cptr->ssl) == X509_V_OK)
      RPR_DL(cptr, "Client sent certificate, which verified OK");
  }
  else
    RPR_DL(cptr, "Certificate not found");
}

void handle_ssl_accept(connection *cptr, u_ns_ts_t now)
{
int r;
  RPR_DL(cptr, "Method called.");

  RPR_DL(cptr, "Clear previous error in ssl through ERR_clear_error()");
  ERR_clear_error();
  RPR_DL(cptr, "do ssl accept");
  r=SSL_accept(cptr->ssl);
  RPR_DL(cptr, "SSL accept:done r=%d", r);
  switch (SSL_get_error(cptr->ssl, r))
  {
    case SSL_ERROR_NONE:
      RPR_DL(cptr, "set_ssl:case no err");
       //check_cert_chain(cptr->ssl, "localhost");
      if(g_ssl_client_authentication)
        check_ssl_client_cert(cptr);
      cptr->state = READ_REQUEST;
      break;
    case SSL_ERROR_WANT_READ:
      RPR_DL(cptr, "set_ssl:case want read err");
      cptr->state = SSL_ACCEPTING;
      return;
    case SSL_ERROR_WANT_WRITE:
      RPR_DL(cptr, "set_ssl:case want write err");
      cptr->state = SSL_ACCEPTING;
      return;
    case SSL_ERROR_SYSCALL:{
      char error[512];
      //list accepted cipher.
      RPR_DL(cptr, "available cipher = %s, last error = %s", SSL_get_cipher_list(cptr->ssl, 0),  ERR_error_string(ERR_get_error(), error));
      RPR_DL(cptr, "set_ssl:case err syscall err=%s", strerror(errno));
      close_fd(cptr, 1, now);
      return;
    }
    case SSL_ERROR_ZERO_RETURN:
      RPR_DL(cptr, "set_ssl:case err zero");
      close_fd(cptr, 1, now);
      return;
    case SSL_ERROR_SSL:
      RPR_DL(cptr, "set_ssl:case err ssl");
      ERR_print_errors_fp(stderr);
      if(g_ssl_client_authentication) //Only to debug log
        check_ssl_client_cert(cptr);
      close_fd(cptr, 1, now);
      return;
    default:
      RPR_DL(cptr, "set_ssl:case default");
      ERR_print_errors_fp(stderr);
      close_fd(cptr, 1, now);
      return;
  }

  if (ssl_regenotiation)
  {
    RPR_DL(cptr, "starting ssl renegotation");
    SSL_renegotiate(cptr->ssl);
    SSL_do_handshake(cptr->ssl);
  }
}

//To initilize and set SSL *ssl variable
void set_ssl(connection *cptr, u_ns_ts_t now)
{
  BIO *sbio;
  RPR_DL(cptr, "Method called.");

  char err[1024] = "";
  cptr->ssl = SSL_new(my_ctx);
  if(!cptr->ssl)
  {
    sprintf(err, "Failed to initialize new SSL connection: %s", ERR_error_string(ERR_get_error(), NULL));
    rpr_berr_exit(err);
  }

  //Make sure that the socket is not closed by the BIO functions
  if(!(sbio = BIO_new_socket(cptr->fd, BIO_NOCLOSE)))
    rpr_berr_exit("BIO_new_socket failed.");
  /* Set up BIOs */
  SSL_set_bio(cptr->ssl, sbio, sbio);

  handle_ssl_accept(cptr, now);
}

char *get_timer_type_by_name(int value)
{
  switch(value)
  {
   case AB_TIMEOUT_IDLE:       return("AB_TIMEOUT_IDLE");
   case AB_TIMEOUT_THINK:      return("AB_TIMEOUT_THINK");
   case AB_TIMEOUT_STHINK:     return("AB_TIMEOUT_STHINK");
   case AB_TIMEOUT_RAMP:       return("AB_TIMEOUT_RAMP");
   case AB_TIMEOUT_END:        return("AB_TIMEOUT_END");
   case AB_TIMEOUT_PROGRESS:   return("AB_TIMEOUT_PROGRESS");
   case AB_TIMEOUT_UCLEANUP:   return("AB_TIMEOUT_UCLEANUP");
   case AB_TIMEOUT_KA:         return("AB_TIMEOUT_KA");
#ifdef RMI_MODE 
   case AB_TIMEOUT_URL_IDLE:   return("AB_TIMEOUT_URL_IDLE");
   case AB_TIMEOUT_RETRY_CONN: return("AB_TIMEOUT_RETRY_CONN");
#endif
   default: return("NA");
  }
}

discrete_timer ab_timers[] =
  {{NULL,NULL,60},
   {NULL,NULL,60},
   {NULL,NULL,1},
   {NULL,NULL,2},
   {NULL,NULL,60},
   {NULL,NULL,10},
   {NULL, NULL,10},
   {NULL, NULL,10}
#ifdef RMI_MODE
   ,{NULL, NULL, 1}
   ,{NULL, NULL, 1}
#endif
  };

/* Function used for Initializing the random number generator */
u_ns_ts_t base_timestamp = 946684800;   /* approx time to substract from 1/1/70 */
  
u_ns_ts_t get_ms_stamp() {
  struct timeval want_time;
  u_ns_ts_t timestamp;


  gettimeofday(&want_time, NULL);


  timestamp = (want_time.tv_sec - base_timestamp)*1000 + (want_time.tv_usec / 1000);


  return timestamp; 
} 

static int ab_timers_count = sizeof(ab_timers)/sizeof(ab_timers[0]);

timer_type* dis_timer_next(u_ns_ts_t now)
{
  int i, type;
  u_ns_ts_t timeout, min;
  timer_type* idx, *tidx;

  RPR_DL(NULL, "Method called. now=%ld, ab_timers_count = %d", now, ab_timers_count);
  RPR_DL(NULL, "sizeof(ab_timers) = %d, sizeof(ab_timers[0]) = %d", sizeof(ab_timers), sizeof(ab_timers[0]));

  /*NS Monitoring: In Kohls a continue monitoring test was ran for 25 days, 
  here "int min = INT_MAX" its value overflow after 25 days. Due to which PTT, session pacing and all other timers 
  were not executed and hence everytime next timer send was NULL. Therefore child process stuck into infinite 
  epoll wait. 
  Solution: Making min variable unsigned long long int*/
  min = ULLONG_MAX;
  idx = NULL;
  for (i = 0; i < ab_timers_count; i++) {
    while (ab_timers[i].next &&
           ab_timers[i].next->timer_type >= 0) {
      timeout = ab_timers[i].next->timeout;

      RPR_DL(NULL, "timeout = %ld", timeout);
      if (timeout <= now) {
        /* Timer has expired */
        tidx = ab_timers[i].next;
        type = tidx->timer_type;
        (tidx->timer_proc)( tidx->client_data, now );
        /* It is possible that that timer has been stopped during execution of callback */
        if ( (tidx->timer_type >=0 ) && (timeout == tidx->timeout) && (type == tidx->timer_type)) {
          dis_timer_del(tidx);
          if ( tidx->periodic )
          {
            if(type == AB_TIMEOUT_RAMP || type ==AB_TIMEOUT_END)
              dis_timer_think_add(type, tidx ,timeout, tidx->timer_proc, tidx->client_data, 1);
            else
              dis_timer_add(type, tidx ,timeout, tidx->timer_proc, tidx->client_data, 1);
          }
        }
        continue;
      }

      RPR_DL(NULL, "timeout = %ld, min = %ld", timeout, min);

      if (timeout < min) {
        min = timeout;
        idx = ab_timers[i].next;
      }
      break;
    }
  }

  //NSDL3_TIMER(vptr, cptr, "STS:next tmr returns 0x%x: \n", (unsigned int)idx);
  return idx;
}

void dis_timer_think_add(int type, timer_type* tmr,
    u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data,
    int periodic )
{ 
  u_ns_ts_t absolute_timeout;   
  timer_type* prev_timer;

  //if (global_settings->non_random_timers)
  //      return (dis_timer_add(type, tmr, now, timer_proc, client_data, periodic ));
  
  RPR_DL(NULL, "Method Called. type=%s, tmr=%p now=%ld val=%d", get_timer_type_by_name(type), tmr, now, tmr->actual_timeout);
  
#if 0
  if (type != AB_TIMEOUT_THINK) {
    fprintf(stderr, "dis_timer_think_add: timer to add must be of type AB_TIMEOUT_THINK\n");
    return;
  }
#endif
  
  //absolute_timeout = now + ab_timers[type].timeout_val;
  absolute_timeout = now + tmr->actual_timeout;
  
  if (tmr->timer_type >= 0) {
    dis_timer_del(tmr);
  }

  tmr->timer_type = type;
  tmr->timer_proc = timer_proc;
  tmr->periodic = periodic;
  tmr->client_data = client_data;
  tmr->timeout = absolute_timeout;

  if (ab_timers[type].prev) {
    prev_timer = ab_timers[type].prev;
    while (prev_timer->timeout > absolute_timeout) {
      prev_timer = prev_timer->prev;
      if (prev_timer == NULL)
        break;
    }
    if (prev_timer) {
      if (prev_timer->next == NULL) { /*Means we got put it at end of the list */
        prev_timer->next = tmr;
        tmr->prev = prev_timer;
        tmr->next = NULL;
        ab_timers[type].prev = tmr;
      } else {  /* Means we put it within the list */
        prev_timer->next->prev = tmr;
        tmr->next = prev_timer->next;
        prev_timer->next = tmr;
        tmr->prev = prev_timer;
      }
    } else { /* Means we got to put it at the front of the list */
      tmr->next = ab_timers[type].next;
      tmr->next->prev = tmr;
      tmr->prev = NULL;
      ab_timers[type].next = tmr;
    }
  }
  else {   /* The list is empty */
    tmr->next = NULL;
    tmr->prev = NULL;
    ab_timers[type].next = tmr;
    ab_timers[type].prev = tmr;
  }

  //NSDL3_TIMER(vptr, cptr, "STS:add tmr: type=%d, tmr=%d ADDED\n", type, (unsigned int)tmr);
}

/*
  remove element from from our timer array
  idx: the index of the element you want to remove
 */
inline void dis_timer_del(timer_type* tmr)
{
  int type = tmr->timer_type;
  RPR_DL(NULL, "Method called. type=%s, tmr=%p, now=%u", get_timer_type_by_name(type), tmr, get_ms_stamp());

  assert(type >= 0);

  if (tmr->next)
    tmr->next->prev = tmr->prev;
  else
    ab_timers[type].prev = tmr->prev;
  if (tmr->prev)
    tmr->prev->next = tmr->next;
  else
    ab_timers[type].next = tmr->next;

  tmr->timer_type = -1;
  RPR_DL(NULL, "STS:del tmr: type=%d, tmr=%p DONE", type, tmr);
}

inline void dis_timer_add(int type, timer_type* tmr,
                                 u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data,
                                 int periodic )
{
  RPR_DL(NULL, "Method called. type=%s, idx=%p now=%u val=%d", get_timer_type_by_name(type), tmr, now, tmr->actual_timeout);

  if(tmr->timer_type >= 0) {
        dis_timer_del(tmr);
  }

  tmr->timer_type = type;
  tmr->timer_proc = timer_proc;
  tmr->periodic = periodic;
  tmr->client_data = client_data;
  //tmr->timeout = now + ab_timers[type].timeout_val;
  tmr->timeout = now + tmr->actual_timeout;

  if (ab_timers[type].prev)
    ab_timers[type].prev->next = tmr;
  else
    ab_timers[type].next = tmr;
  tmr->prev = ab_timers[type].prev;
  tmr->next = NULL;
  ab_timers[type].prev = tmr;
  //NSDL3_TIMER(vptr, cptr, "STS:add tmr: type=%d, idx=0x%x ADDED\n", type, (unsigned int)tmr);
}

/* Execute the already expired timers */
void dis_timer_run_ex(u_ns_ts_t now, int tmr_type)
{
  register int i, /*min,*/ type, start_idx, end_idx;
  register u_ns_ts_t timeout;
  //register timer_type* idx;
  register timer_type* tidx;

  RPR_DL(NULL, "Method called. now=%u", now);

  //min = INT_MAX;
  //idx = NULL;
  if (tmr_type == -1)
  {
    start_idx = 0;
    end_idx = ab_timers_count;
  }
  else if (tmr_type < ab_timers_count)
  {
    start_idx = tmr_type;
    end_idx = tmr_type + 1;
  }
  else
  {
     //TODO
     return;
  }

  for (i = start_idx; i < end_idx; i++) {
    while (ab_timers[i].next &&
           ab_timers[i].next->timer_type >= 0) {
      timeout = ab_timers[i].next->timeout;
      if (timeout <= now) {
        /* Timer has expired */
        tidx = ab_timers[i].next;
        type = tidx->timer_type;

        (tidx->timer_proc)( tidx->client_data, now );
        /* It is possible that that timer has been stopped during execution of callback */
        if ((tidx->timer_type >=0 ) && (timeout == tidx->timeout) && (type == tidx->timer_type)) {
          dis_timer_del(tidx);
          if ( tidx->periodic )
          {
            if(type == AB_TIMEOUT_RAMP || type ==AB_TIMEOUT_END)
              dis_timer_think_add(type, tidx ,timeout, tidx->timer_proc, tidx->client_data, 1);
            else
              dis_timer_add(type, tidx ,timeout, tidx->timer_proc, tidx->client_data, 1);
          }
        }
        continue;
      } else
        break;
    }
  }
  return;
}

void dis_timer_run(u_ns_ts_t now)
{
  dis_timer_run_ex(now, -1);
}

/*If call_optimzed = 1, then call optimized
 *  else non optimized timer add function*/
inline void dis_timer_add_ex(int type, timer_type* tmr,
                                 u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data,
                                 int periodic, int call_optimized)
{
  RPR_DL(NULL, "Method called");
  if(call_optimized)
  {
    RPR_DL(NULL, "Setting optimized timer");
    dis_timer_add(type, tmr, now, timer_proc, client_data, periodic);
  }
  else
  {
    RPR_DL(NULL, "Setting nonoptimized timer(sorted)");
    dis_timer_think_add(type, tmr, now, timer_proc, client_data, periodic);
  }
}

/*REset timer for DLE connection. This function
 * will get call only for IDLE timer*/
inline void dis_idle_timer_reset (u_ns_ts_t now, timer_type* tmr)
{
  int type, periodic;
  RPR_DL(NULL, "Method called. tmr=%p: now=%u", tmr, now);

  type = tmr->timer_type;
  periodic = tmr->periodic;

  RPR_DL(NULL, "RESET TIMER: type = %d, timeout = %llu, actual_timeout = %d, periodic = %d, timer_status = %d",
                           type, tmr->timeout, tmr->actual_timeout, tmr->periodic, tmr->timer_status);

  dis_timer_del(tmr);
  dis_timer_add_ex(type, tmr ,now, tmr->timer_proc,
                tmr->client_data, periodic, 0);
                //tmr->client_data, periodic, global_settings->idle_timeout_all_flag);
  //NSDL3_TIMER(vptr, cptr, "STS:reset tmr=0x%x Done: \n", (unsigned int) tmr);
}

