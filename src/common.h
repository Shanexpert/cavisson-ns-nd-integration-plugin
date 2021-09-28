#ifndef _common_h
#define _common_h

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>

#include <openssl/ssl.h>

#define CA_LIST         "root.pem"
#define HOST            "localhost"
#define RANDOM_FILE     "random.pem"

#define PORT	4433
#define BUFSIZZ 1024

extern BIO *bio_err;
int berr_exit (char *string);
int err_exit(char *string);

extern SSL_CTX *initialize_ctx(char *keyfile, char *password);
extern SSL_CTX *initialize_ctx_clients(char *keyfile);
void destroy_ctx(SSL_CTX *ctx);
#endif
