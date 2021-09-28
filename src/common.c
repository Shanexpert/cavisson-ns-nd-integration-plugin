#include <string.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include "common.h"

BIO *bio_err=0;
static char *pass;
static int password_cb(char *buf,int num,int rwflag,void *userdata);
//static void sigpipe_handle(int x);

/* A simple error and exit routine*/
int err_exit(string)
  char *string;
  {
    fprintf(stderr,"%s\n",string);
    exit(0);
  }

/* Print SSL errors and exit*/
int berr_exit(string)
  char *string;
  {
    BIO_printf(bio_err,"%s\n",string);
    ERR_print_errors(bio_err);
    exit(0);
  }

/*The password code is not thread safe*/
static int password_cb(char *buf,int num,int rwflag,void *userdata)
  {
    if(num<strlen(pass)+1)
      return(0);

    strcpy(buf,pass);
    return(strlen(pass));
  }
/*static void sigpipe_handle(int x){
	printf("Rcd sig = %d\n", x);
}*/

//for client 
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
SSL_CTX *initialize_ctx_clients(char *keyfile)
{
  //SSL_METHOD *meth;
  SSL_CTX *ctx;

  //Global system initialization
  //SSL_library_init();
  //OPENSSL_init_ssl();
  //SSL_load_error_strings();
 
  // An error write context
  bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);
  
  // Create our method
  //meth=(SSL_METHOD *)TLS_method();
  
  // Create our context
  ctx=SSL_CTX_new(TLS_method());
  
  // Load our keys and certificates
  if(!(SSL_CTX_use_certificate_file(ctx,keyfile,SSL_FILETYPE_PEM)))
  berr_exit("Couldn't read certificate file");
  
  if(!(SSL_CTX_use_PrivateKey_file(ctx,keyfile,SSL_FILETYPE_PEM)))
  berr_exit("Couldn't read key file");
  
  return ctx;
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L
SSL_CTX *initialize_ctx_clients(char *keyfile)
{
  SSL_METHOD *meth;
  SSL_CTX *ctx;

  //Global system initialization
  SSL_library_init();
  SSL_load_error_strings();

  // An error write context
  bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);

  // Create our method
  meth=(SSL_METHOD *)SSLv23_method();

  // Create our context
  ctx=SSL_CTX_new(meth);

  // Load our keys and certificates
  if(!(SSL_CTX_use_certificate_file(ctx,keyfile,SSL_FILETYPE_PEM)))
  berr_exit("Couldn't read certificate file");

  if(!(SSL_CTX_use_PrivateKey_file(ctx,keyfile,SSL_FILETYPE_PEM)))
  berr_exit("Couldn't read key file");

  return ctx;
}
#endif

//for server
SSL_CTX *initialize_ctx(keyfile,password)
  char *keyfile;
  char *password;
  {
    const SSL_METHOD *meth = NULL;
    SSL_CTX *ctx;
    char *ptr;
    char buf[1024];
    
    ptr = getenv("NS_WDIR");
    if (!ptr) {
	printf("NS_WDIR env variable must be defined\n");
	exit (1);
    }
    if(!bio_err){
      /* Global system initialization*/
      SSL_library_init();
      SSL_load_error_strings();
      
      /* An error write context */
      bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);
    }

    /* Set up a SIGPIPE handler */
    //signal(SIGPIPE,sigpipe_handle);
    
    /* Create our context*/
    //meth=SSLv3_method();
    //meth=SSLv23_method();
   // meth = (SSL_METHOD *)SSLv23_method();
     #if OPENSSL_VERSION_NUMBER >= 0x10100000L
    meth=TLS_method();
    ctx=SSL_CTX_new(meth);
    #else
    meth=SSLv23_server_method();
    ctx=SSL_CTX_new(meth);
    #endif

    /* Load our keys and certificates*/
    //printf("Loading Certificate from %s into ctx", keyfile);
    //if(!(SSL_CTX_use_certificate_chain_file(ctx, keyfile)))

    // Following apis was not loading chain added in certificate, hence replaced by SSL_CTX_use_certificate_chain_file
    //if(!(SSL_CTX_use_certificate_file(ctx,keyfile,SSL_FILETYPE_PEM)))
    int ret;
    ret = SSL_CTX_use_certificate_chain_file(ctx, keyfile);

    switch (ret)
    {
      case 1:
        printf("SSL certificate (%s) load ok\n", keyfile);
        break;
      default:
        fprintf(stderr, "SSL cert load error [%s]\n", ERR_error_string(ERR_get_error(), NULL));
        berr_exit("Cannot load certificate file");
    }

    //printf("Certificate has loaded from %s into ctx", keyfile);

    pass=password;
    SSL_CTX_set_default_passwd_cb(ctx,password_cb);
    // if (!(SSL_CTX_use_RSAPrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM)))
    if(!(SSL_CTX_use_PrivateKey_file(ctx,keyfile,SSL_FILETYPE_PEM)))
      berr_exit("Couldn't read key file");

    if (!SSL_CTX_check_private_key(ctx))
      berr_exit("Key is not valid");
    /* Load the CAs we trust*/
    sprintf (buf, "%s/cert/%s", ptr, CA_LIST);
    if(!(SSL_CTX_load_verify_locations(ctx,buf,0)))
      berr_exit("Couldn't read CA list");
    SSL_CTX_set_verify_depth(ctx,1);
    /* Load randomness */
    sprintf (buf, "%s/cert/%s", ptr, RANDOM_FILE);
    if(!(RAND_load_file(buf,1024*1024)))
      berr_exit("Couldn't load randomness");
       
    return ctx;
  }


void destroy_ctx(ctx)
  SSL_CTX *ctx;
  {
    SSL_CTX_free(ctx);
  }
