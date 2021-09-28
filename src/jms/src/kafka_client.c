#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>
#include "ns_string.h"
#include "/opt/librdkafka-master/src/rdkafka.h"

#define MAX 1024*120
#define CLEAR_WHITE_SPACE_FROM_END(ptr) { int end_len = strlen(ptr); \ 
				          while((ptr[end_len - 1] == ' ') || ptr[end_len - 1] == '\t') { \
					    ptr[end_len - 1] = '\0'; \
					    end_len = strlen(ptr); \
					  }\
                          	 	}
pthread_mutex_t producer_mutex;
char buf[MAX] = "";
//static rd_kafka_t *rk[10];
static int quiet = 0;
char ip[10][20];
char topic[10][16];

//Kafka logger callback (optional)
static void logger (const rd_kafka_t *rk, int level, const char *fac, const char *buf) 
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  fprintf(stderr, "%u.%03u RDKAFKA-%i-%s: %s: %s\n", (int)tv.tv_sec, (int)(tv.tv_usec / 1000),
            	   level, fac, rk ? rd_kafka_name(rk) : NULL, buf);
}

/**
 * Message delivery report callback.
 * Called once for each message.
 * See rdkafka.h for more information.
 */
static void msg_delivered (rd_kafka_t *rk, void *payload, size_t len, int error_code, void *opaque, void *msg_opaque)
{
  fprintf(stderr, "in msg_delivered\n");
  if (error_code)
    fprintf(stderr, "%% Message delivery failed: %s\n", rd_kafka_err2str(error_code));
  else if (!quiet)
    fprintf(stderr, "%% Message delivered (%zd bytes): %.*s\n", len, (int)len, (const char *)payload);
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
  //return i;
}
void *kafka_call_back(void *indx)
{
  
  rd_kafka_topic_t *rkt;
  int partition = RD_KAFKA_PARTITION_UA;
  rd_kafka_conf_t *conf;
  rd_kafka_topic_conf_t *topic_conf;
  char errstr[512];
  char tmp[16];
  int run = 1; 
  int idx = *((int *)indx);
  rd_kafka_t *rk;
  int len = 0;

  /* Kafka configuration */
  conf = rd_kafka_conf_new();
  
  /* Set logger */
  rd_kafka_conf_set_log_cb(conf, logger);

  /* Quick termination */
  snprintf(tmp, sizeof(tmp), "%i", SIGIO);
  rd_kafka_conf_set(conf, "internal.termination.signal", tmp, NULL, 0);

  /* Topic configuration */
  topic_conf = rd_kafka_topic_conf_new();

  /* Set up a message delivery report callback.
  * It will be called once for each message, either on successful
  * delivery to broker, or upon failure to deliver to broker. */

  /* If offset reporting (-o report) is enabled, use the
   * richer dr_msg_cb instead. */
   //rd_kafka_conf_set_dr_cb(conf, msg_delivered);

  /* Create Kafka handle */
  if (!(rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf,
                          errstr, sizeof(errstr)))) {
     fprintf(stderr, "%% Failed to create new producer: %s\n", errstr);
     return ;
  }
  rd_kafka_set_log_level(rk, LOG_DEBUG);

  /* Add brokers */
  if (rd_kafka_brokers_add(rk, ip[idx]) == 0)
  {
    fprintf(stderr, "%% No valid brokers specified\n"); 
    return ;
  }

  /* Create topic */
  rkt = rd_kafka_topic_new(rk, topic[idx], topic_conf);
  topic_conf = NULL; /* Now owned by topic */

   //while(1)
   {
     pthread_mutex_lock(&producer_mutex); 
     //ns_advance_param("shipments");
     
     if(!strcmp(topic[idx], "of_orders"))
       strcpy(buf, ns_eval_string("{orders}"));
     else
       strcpy(buf, ns_eval_string("{shipments}"));

     strcpy(buf, ns_eval_string(buf));
     len = strlen(buf);

     //fprintf(stderr, "ip = %s, topic = %s, idx = %d, len = %d\n", ip[idx], topic[idx], idx, len);
     ns_start_transaction("kafka_producer");
     /* Send/Produce message. */
     if (rd_kafka_produce(rkt, partition, RD_KAFKA_MSG_F_COPY,
                             /* Payload and length */
                             buf, len,
                             /* Optional key and its length */
                             NULL, 0,
                             /* Message opaque, provided in
                              * delivery report callback as
                              * msg_opaque. */
                             NULL) == -1) {
       fprintf(stderr, "%% Failed to produce to topic %s "
		      "partition %i: %s\n", rd_kafka_topic_name(rkt), partition,
                      rd_kafka_err2str(rd_kafka_last_error()));
       /* Poll to handle delivery reports */
       ns_end_transaction_as("kafka_producer", NS_AUTO_STATUS, "kafka_producer_Error");
       rd_kafka_poll(rk, 0);
     }
     else
       ns_end_transaction_as("kafka_producer", NS_AUTO_STATUS, "kafka_producer_Sucess");
     //pthread_mutex_unlock(&producer_mutex);
   }

//#if 0
  /* Wait for messages to be delivered */
   //while (run && rd_kafka_outq_len(rk) > 0)
     //rd_kafka_poll(rk, 100);

  /* Destroy topic */
  rd_kafka_topic_destroy(rkt);

  /* Destroy the handle */
  rd_kafka_destroy(rk);
  
  if (topic_conf)
    rd_kafka_topic_conf_destroy(topic_conf);

  /* Let background threads clean up and terminate cleanly. */
 /* run = 5;
  while (run-- > 0 && rd_kafka_wait_destroyed(1000) == -1)
    printf("Waiting for librdkafka to decommission\n");
  if (run <= 0)
    rd_kafka_dump(stdout, rk); */

 pthread_mutex_unlock(&producer_mutex);
//#endif
  return ;
}

void kafka_client()
{
  pthread_t thread_id[10];
  pthread_attr_t attr;
  int ret = 0;   //return the thread value
  int idx[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  FILE *fp = NULL;
  char line[50];  
  char *ptr = NULL;
  int i = 0, j = 0;
  char *fields[2];
   
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  //ip_data_file.txt consist of ip:port and topic name seperated by comma operator eg. 10.10.30.13:9092,order
  if ((fp = fopen("/home/netstorm/BigData/scripts/kafka/kafka/kafka_producer/ip_data_file.txt", "r")) != NULL)
  {
    while(nslb_fgets(line, 50, fp, 0) != NULL)
    {
      if(line[0] == '\n' || line[0] == '#') {
        line[0] = '\0';
        continue;
      }

      if((ptr = strchr(line, '\n')) != NULL)
        *ptr = '\0';
      get_tokens(line, fields, ",");
     
      strcpy(ip[i], fields[0]);
      strcpy(topic[i], fields[1]);
      
      CLEAR_WHITE_SPACE_FROM_END(topic[i]);  
      ret = pthread_create(&thread_id[i], &attr, kafka_call_back, (void *)&idx[i]);
      i++;
      if(ret)
      {
        fprintf(stderr, "ERROR: return code from pthread_create is = %d \n", ret);
        return ;
      }
    }
    fclose(fp);
  }
  else {
   fprintf(stderr, "file doesn't exist\n");
   return ;
  }

  pthread_attr_destroy(&attr);
  for (j = 0; j < i; j++)
  {
    if(pthread_join(thread_id[j], NULL))
    {
      fprintf(stderr, "Error joining thread\n");
      return ;
    }
  }
  return ;
}
