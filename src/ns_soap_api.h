#ifndef _NS_SOAP_API_H
#define _NS_SOAP_API_H
#define SOAP_VAR_BUF_SIZE 128
#define SOAP_SIG_BUF_SIZE 1024
#define SOAP_FILE_NAME_SIZE 1024
#define SOAP_FILE_BUF_SIZE 4096

/*Structure of SOAP WS Security*/
typedef struct nsSoapWSSecurityInfo
{
  char keyFile[SOAP_FILE_NAME_SIZE+1];
  char *key; /*Private Key*/
  char *certificate; /*Public Certificate*/
  int  algorithm; /*Digest Algorithms*/
  char token[SOAP_VAR_BUF_SIZE+1]; /*Digest Token*/
  char digest_id[SOAP_VAR_BUF_SIZE+1]; /*Digest Element ID*/
  char cert_id[SOAP_VAR_BUF_SIZE+1];/*Certificate ID*/
  char sign_id[SOAP_VAR_BUF_SIZE+1];/*Signature ID*/
  char key_info_id[SOAP_VAR_BUF_SIZE+1];/*Key Info ID*/
  char token_id[SOAP_VAR_BUF_SIZE+1];/*Security Token ID*/
}nsSoapWSSecurityInfo;

/*SOPA API*/
nsSoapWSSecurityInfo* ns_add_soap_ws_security(char *keyFile, char *certFile, int algorithm, char *token, char *digest_id, char *cert_id, char *sign_id, char *key_info_id, char* sec_token_id );

int ns_update_soap_ws_security(nsSoapWSSecurityInfo *ns_ws_info, char *keyFile, char *certFile, int algorithm, char *token, char *digest_id, char *cert_id, char *sign_id, char *key_info_id, char* token_id);

int ns_apply_soap_ws_security(nsSoapWSSecurityInfo *ns_ws_info , char *buffer, int buf_len , char **outbuf , int *outbuf_len);

int ns_free_soap_ws_security(nsSoapWSSecurityInfo *ns_ws_info);
#endif
