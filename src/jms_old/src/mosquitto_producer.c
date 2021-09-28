#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <process.h>
#include <winsock2.h>
#define snprintf sprintf_s
#endif
#include <mosquitto.h>
#include "client_shared.h"
#include "ns_string.h"

#define STATUS_CONNECTING 0
#define STATUS_CONNACK_RECVD 1
#define STATUS_WAITING 2

#define CLEAR_WHITE_SPACE_FROM_END(ptr) { int end_len = strlen(ptr); \
                                          while((ptr[end_len - 1] == ' ') || ptr[end_len - 1] == '\t') { \
                                            ptr[end_len - 1] = '\0';\
                                            end_len = strlen(ptr);\
                                          }\
                                        }

/* Global variables for use in callbacks. See sub_client.c for an example of
 * using a struct to hold variables for use in callbacks. */
static char topic[120];
static int mid_sent;
static bool connected = true;
static char username[60];
static char password[60];
static bool disconnect_sent = false;
static bool quiet = false;

void init_config(struct mosq_config *cfg)
{
        cfg->port = 1883;
        cfg->max_inflight = 20;
        cfg->keepalive = 60;
        cfg->clean_session = true;
        cfg->eol = true;
        cfg->protocol_version = MQTT_PROTOCOL_V31;
	cfg->topic = topic;
        //cfg->message = "hello";
        //cfg->msglen = 6;
	cfg->username = username;
        cfg->password = password;
	cfg->id = username;
        cfg->host = "iot-qw-stress.tst.kohls.com";
        cfg->debug = false;
        connected = true;
        disconnect_sent = false;
}

void client_config_cleanup(struct mosq_config *cfg)
{
        int i;
        free(cfg->id);
        free(cfg->id_prefix);
        free(cfg->host);
        free(cfg->file_input);
        //free(cfg->message);
        free(cfg->topic);
        free(cfg->bind_address);
        free(cfg->username);
        free(cfg->password);
        free(cfg->will_topic);
        free(cfg->will_payload);
#ifdef WITH_TLS
        free(cfg->cafile);
        free(cfg->capath);
        free(cfg->certfile);
        free(cfg->keyfile);
        free(cfg->ciphers);
        free(cfg->tls_version);
#  ifdef WITH_TLS_PSK
        free(cfg->psk);
        free(cfg->psk_identity);
#  endif
#endif
        if(cfg->topics){
                      for(i=0; i<cfg->topic_count; i++){
                        free(cfg->topics[i]);
                }
                free(cfg->topics);
        }
        if(cfg->filter_outs){
                for(i=0; i<cfg->filter_out_count; i++){
                        free(cfg->filter_outs[i]);
                }
                free(cfg->filter_outs);
        }
#ifdef WITH_SOCKS
        free(cfg->socks5_host);
        free(cfg->socks5_username);
        free(cfg->socks5_password);
#endif
}

int client_id_generate(struct mosq_config *cfg, const char *id_base)
{
        int len;
        char hostname[256];

        if(cfg->id_prefix){
                cfg->id = malloc(strlen(cfg->id_prefix)+10);
                if(!cfg->id){
                        if(!cfg->quiet) fprintf(stderr, "Error: Out of memory.\n");
                        return 1;
                }
                snprintf(cfg->id, strlen(cfg->id_prefix)+10, "%s%d", cfg->id_prefix, getpid());
        }else if(!cfg->id){
                hostname[0] = '\0';
                gethostname(hostname, 256);
                hostname[255] = '\0';
                len = strlen(id_base) + strlen("/-") + 6 + strlen(hostname);
                cfg->id = malloc(len);
                if(!cfg->id){
                        if(!cfg->quiet) fprintf(stderr, "Error: Out of memory.\n");
                        return 1;
                }
                snprintf(cfg->id, len, "%s/%d-%s", id_base, getpid(), hostname);
                if(strlen(cfg->id) > MOSQ_MQTT_ID_MAX_LENGTH){
                        /* Enforce maximum client id length of 23 characters */
                        cfg->id[MOSQ_MQTT_ID_MAX_LENGTH] = '\0';
                }
        }
        return MOSQ_ERR_SUCCESS;
}

int client_opts_set(struct mosquitto *mosq, struct mosq_config *cfg)
{

        if(cfg->will_topic && mosquitto_will_set(mosq, cfg->will_topic,
                                cfg->will_payloadlen, cfg->will_payload, cfg->will_qos,
                                cfg->will_retain)){

                if(!cfg->quiet) fprintf(stderr, "Error: Problem setting will.\n");
                return 1;
        }
        if(cfg->username && mosquitto_username_pw_set(mosq, cfg->username, cfg->password)){
                if(!cfg->quiet) fprintf(stderr, "Error: Problem setting username and password.\n");
                return 1;
        }
#ifdef WITH_TLS
        if((cfg->cafile || cfg->capath)
                        && mosquitto_tls_set(mosq, cfg->cafile, cfg->capath, cfg->certfile, cfg->keyfile, NULL)){

                if(!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS options.\n");
                return 1;
        }
        if(cfg->insecure && mosquitto_tls_insecure_set(mosq, true)){
                if(!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS insecure option.\n");
                return 1;
        }
#  ifdef WITH_TLS_PSK
        if(cfg->psk && mosquitto_tls_psk_set(mosq, cfg->psk, cfg->psk_identity, NULL)){
                if(!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS-PSK options.\n");
                return 1;
        }
#  endif
        if(cfg->tls_version && mosquitto_tls_opts_set(mosq, 1, cfg->tls_version, cfg->ciphers)){
if(!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS options.\n");
                return 1;
        }
#endif
        mosquitto_max_inflight_messages_set(mosq, cfg->max_inflight);
#ifdef WITH_SOCKS
        if(cfg->socks5_host){
                rc = mosquitto_socks5_set(mosq, cfg->socks5_host, cfg->socks5_port, cfg->socks5_username, cfg->socks5_password);
                if(rc){
                        return rc;
                }
        }
#endif
        mosquitto_opts_set(mosq, MOSQ_OPT_PROTOCOL_VERSION, &(cfg->protocol_version));
        return MOSQ_ERR_SUCCESS;
}

int client_connect(struct mosquitto *mosq, struct mosq_config *cfg)
{
        char err[1024];
        int rc;

#ifdef WITH_SRV
        if(cfg->use_srv){
                rc = mosquitto_connect_srv(mosq, cfg->host, cfg->keepalive, cfg->bind_address);
        }else{
                rc = mosquitto_connect_bind(mosq, cfg->host, cfg->port, cfg->keepalive, cfg->bind_address);
        }
#else
        rc = mosquitto_connect_bind(mosq, cfg->host, cfg->port, cfg->keepalive, cfg->bind_address);
#endif
        if(rc>0){
                if(!cfg->quiet){
                        if(rc == MOSQ_ERR_ERRNO){
#ifndef WIN32
                                strerror_r(errno, err, 1024);
#else
                                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errno, 0, (LPTSTR)&err, 1024, NULL);
#endif
                                fprintf(stderr, "Error: %s\n", err);
                        }else{
                                fprintf(stderr, "Unable to connect (%s).\n", mosquitto_strerror(rc));
                        }
                }
                return rc;
        }
        return MOSQ_ERR_SUCCESS;
}


void my_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
  char message[2048];
  //fprintf(stderr, "in my_connect_callback, result = %d\n", result);
  int rc = MOSQ_ERR_SUCCESS;

  if(!result){
    ns_start_transaction("Mosquitto_Publisher");
    ns_advance_param("message_list");
    strcpy(message, ns_eval_string("{message_list}")); 
    strcpy(message, ns_eval_string(message));
    rc = mosquitto_publish(mosq, &mid_sent, topic, strlen(message), message, 0, 0);
    //ns_end_transaction("Mosquitto_Publisher", NS_AUTO_STATUS);
    if(rc){
      //fprintf(stderr, "in RC check, RC = %d\n", rc);
      if(!quiet){
	switch(rc){
          case MOSQ_ERR_INVAL:
            fprintf(stderr, "Error: Invalid input. Does your topic contain '+' or '#'?\n");
	    break;
          case MOSQ_ERR_NOMEM:
	    fprintf(stderr, "Error: Out of memory when trying to publish message.\n");
	    break;
          case MOSQ_ERR_NO_CONN:
	    fprintf(stderr, "Error: Client not connected when trying to publish.\n");
	    break;
          case MOSQ_ERR_PROTOCOL:
            fprintf(stderr, "Error: Protocol error when communicating with broker.\n");
	    break;
          case MOSQ_ERR_PAYLOAD_SIZE:
	    fprintf(stderr, "Error: Message payload is too large.\n");
	    break;
	}
      }
      mosquitto_disconnect(mosq);
    }
  }else{
   //fprintf(stderr, "else............\n");
   if(result && !quiet){
     fprintf(stderr, "%s\n", mosquitto_connack_string(result)); 
    }
  }
}

void my_disconnect_callback(struct mosquitto *mosq, void *obj, int rc)
{
	//fprintf(stderr, "my_disconnect_callback\n");
	connected = false;
}

void my_publish_callback(struct mosquitto *mosq, void *obj, int mid)
{
	//fprintf(stderr, "my_publish_callback\n");

         if(disconnect_sent == false){
		mosquitto_disconnect(mosq);
		disconnect_sent = true;
	}
}

void my_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str)
{	
  printf("%s\n", str);
}

int get_tokens(char *read_buf, char *fields[], char *token)
{
  char *ptr;
  char *token_ptr = NULL;
  int i = 0;

  ptr = read_buf;
  while((token_ptr = strtok(ptr, token)) != NULL)
  {
    ptr = NULL;
    fields[i] = token_ptr;
    i++;
  }
  return i;
}

void mosquitto_client(int idx)
{
	struct mosq_config cfg;
	struct mosquitto *mosq = NULL;
	int rc = 0;

        //fprintf(stderr, "mosquitto_client idx = %d\n", idx);
        memset(&cfg, 0, sizeof(struct mosq_config));

        init_config(&cfg);
        
	//if(client_id_generate(&cfg, "mosqpub"))
          //return;
        
        //fprintf(stderr, "after client_id_generate\n");
	mosq = mosquitto_new(cfg.id, true, (void *)(&idx));
	if(!mosq){
 	  switch(errno){
	    case ENOMEM:
	      if(!quiet) fprintf(stderr, "Error: Out of memory.\n");
	        break;
            case EINVAL:
	      if(!quiet) fprintf(stderr, "Error: Invalid id.\n");
		break;
	  }
	  return;
	}

	if(cfg.debug)
	  mosquitto_log_callback_set(mosq, my_log_callback);
        
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_disconnect_callback_set(mosq, my_disconnect_callback);
	mosquitto_publish_callback_set(mosq, my_publish_callback);

	if(client_opts_set(mosq, &cfg))
	  return;
 
	rc = client_connect(mosq, &cfg);
	if(rc) return;

        //fprintf(stderr, "after client_connect\n");
	
	do{
	  rc = mosquitto_loop(mosq, -1, 1);
          //fprintf(stderr, "after mosquitto_loop RC = %d, connected = %d, idx = %d\n", rc, connected, idx); 
	  }while(rc == MOSQ_ERR_SUCCESS && connected);

	mosquitto_destroy(mosq);
        
	if(rc) {
          ns_end_transaction_as("Mosquitto_Publisher", NS_AUTO_STATUS, "Mosquitto_Publisher_Error");
	  fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        }
   	else
    	  ns_end_transaction_as("Mosquitto_Publisher", NS_AUTO_STATUS, "Mosquitto_Publisher_Success");

	return;
}

void mosquitto_producer()
{
  FILE *fp = NULL;
  char line[240];
  char *ptr = NULL;
  int i = 0;
  char *fields[3];

  //user_file.txt consists of username, password amd topic name sepearted by comma eg. netstorm,netstorm,v1/
  if ((fp = fopen("/home/netstorm/BigData/scripts/Mqtt/Mqtt/mosquitto_producer/user_file.txt", "r")) != NULL)
  {
    while(nslb_fgets(line, 240, fp, 0) != NULL)
    {
      if(line[0] == '\n' || line[0] == '#') {
        line[0] = '\0';
        continue;
      }

      if((ptr = strchr(line, '\n')) != NULL)
        *ptr = '\0';

      //fprintf(stderr, "line = %s\n", line);
      get_tokens(line, fields, ",");

      strcpy(username, fields[0]);
      strcpy(password, fields[1]);
      strcpy(topic, fields[2]);

      //fprintf(stderr, "username[i] = [%s], password[i] = [%s], topic[i] = [%s], idx = %d\n", username, password, topic, i);
      CLEAR_WHITE_SPACE_FROM_END(topic);
      
      mosquitto_client(i);      
      i++;
    }
    fclose(fp);
  }
  else {
   fprintf(stderr, "file doesn't exist\n");
   return ;
  }
  return ;
}
