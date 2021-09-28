#ifndef SSL_SETTINGS_H
#define SSL_SETTINGS_H

/******************************************************************
 * Name    :    ns_ssl.h
 * Purpose :    This file contains methods related to SSL
 * Note    :
 * Author  :    Archana
 * Intial version date:    19/06/08
 * Last modification date: 19/06/08
*****************************************************************/

#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef CAV_SSL_ATTACK
#include <openssl/cav_ssl_attack.h>
#endif

//values for ssl_mode
#define NS_SSL_UNINIT 0
#define NS_SSL_NO_REUSE 1
#define NS_SSL_REUSE 2

#define CIPHER_BUF_MAX_SIZE 4096
#define MAX_SSL_FILE_LENGTH 1024
#define DELTA_CERT_KEY_ENTRIES 16
#define SSL_CERT_KEY_TABLE_SIZE 2048

#define CERT_FILE 0
#define KEY_FILE 1

typedef struct SSLExtraCert
{
  void *ssl_cert_key_addr;
  struct SSLExtraCert *next;
} SSLExtraCert;

typedef struct
{
  void *ssl_cert_key_addr; 
  struct SSLExtraCert *extra_cert;
  int ssl_cert_key_file_size;
} SSLCertKeyData;

typedef struct
{
  void *ssl_cert_key_addr;                        
  int ssl_cert_key_file_size;                    
} SSLCertKeyData_Shr;

extern SSLExtraCert *ssl_extra_cert;
extern SSLCertKeyData *ssl_cert_key_data;
extern SSLCertKeyData_Shr *ssl_cert_key_data_shr;

extern int kw_set_avg_ssl_reuse(char *buf, int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_ssl_cipher_list(char *buf, char *to_change, int runtime_flag, char *err_msg);
extern int kw_g_set_ssl_cert_file();
extern int kw_set_ssl_key_file();
extern int kw_set_ssl_clean_close_only(char *buf, short *to_change, char *err_msg, int runtime_flag);
extern void set_ssl_default();
extern void ssl_data_check();
extern void kw_set_ssl_attack_file(char *keyword, char *buf);
extern int kw_set_ssl_settings(char *buf, int *to_change, int runtime_flag, char *err_msg);
extern int kw_enable_post_handshake_auth();
extern int kw_set_tls_version();
extern int start_ssl_renegotiation();
extern void kw_set_ssl_key_log(char *buf);
extern int kw_set_host_tls_version(char *keyword, char *buf, char *err_msg, int runtime_flag);
extern int ns_parse_set_ssl_setting(char *buffer); 
extern int ns_set_ssl_setting_ex(); 
extern int ns_unset_ssl_setting_ex(); 
extern int set_keylog_file(SSL_CTX *ctx, const char *keylog_file);
//extern void handle_pop3_ssl_write (connection *cptr, u_ns_ts_t now);
//extern void handle_imap_ssl_write (connection *cptr, u_ns_ts_t now);
//extern inline void check_cert_chain(SSL *ssl, char *host);

#define SSL2_3        0
#define SSL3_0        1
#define TLS1_0        2
#define TLS1_1        3
#define TLS1_2        4
#define TLS1_3        5

#endif
