#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>

#include "ns_kafka_api.h"

static int msg_delivery_flag;

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
static void msg_delivered(rd_kafka_t *rk, void *payload, size_t len, int error_code, void *opaque, void *msg_opaque)
{
  msg_delivery_flag = 1;
}

int ns_make_kafka_producer_connection(NSKafkaClientConn *ns_kaka_conn_ptr, char *server_ip, char *topic_name)
{
  #ifdef NS_DEBUG_ON
    fprintf(stderr, "Method Entry. ns_make_kafka_connection()\n");
  #endif
 
  ns_kaka_conn_ptr->partition = RD_KAFKA_PARTITION_UA;
  char errstr[512];
  char tmp[16];
  int run = 1;

  ns_kaka_conn_ptr->conf = rd_kafka_conf_new();

  snprintf(tmp, sizeof(tmp), "%i", SIGIO);
  rd_kafka_conf_set(ns_kaka_conn_ptr->conf, "internal.termination.signal", tmp, NULL, 0);

  ns_kaka_conn_ptr->topic_conf = rd_kafka_topic_conf_new();

  /* Set up a message delivery report callback.
  * It will be called once for each message, either on successful
  * delivery to broker, or upon failure to deliver to broker. */

  /* If offset reporting (-o report) is enabled, use the
   * richer dr_msg_cb instead. */

  rd_kafka_conf_set_dr_cb(ns_kaka_conn_ptr->conf, msg_delivered);

  /* Create Kafka handle */
  if (!(ns_kaka_conn_ptr->rk = rd_kafka_new(RD_KAFKA_PRODUCER, ns_kaka_conn_ptr->conf, errstr, sizeof(errstr)))) 
  {
     fprintf(stderr, "Error: Failed to create new producer: %s\n", errstr);
     return NS_KAFKA_ERROR;
  }

  /* Add brokers */
  if (rd_kafka_brokers_add(ns_kaka_conn_ptr->rk, server_ip) == 0)
  {
    fprintf(stderr, "Error: No valid brokers specified\n");
    return NS_KAFKA_ERROR;
  }

  /* Create topic */
  ns_kaka_conn_ptr->rkt = rd_kafka_topic_new(ns_kaka_conn_ptr->rk, topic_name, ns_kaka_conn_ptr->topic_conf);
  ns_kaka_conn_ptr->topic_conf = NULL; /* Now owned by topic */

  return NS_KAFKA_SUCCESS;
} 

int ns_kafka_produce_msg(NSKafkaClientConn *ns_kaka_conn_ptr, char *msg)
{
  #ifdef NS_DEBUG_ON
    fprintf(stderr, "Method Entry. ns_kafka_produce_msg()\n");
  #endif

  int len = strlen(msg);

  /* Send/Produce message. */
  if (rd_kafka_produce(ns_kaka_conn_ptr->rkt, ns_kaka_conn_ptr->partition, RD_KAFKA_MSG_F_COPY,
                          /* Payload and length */
                          msg, len,
                          /* Optional key and its length */
                          NULL, 0,
                          /* Message opaque, provided in
                           * delivery report callback as
                           * msg_opaque. */
                          NULL) != 0) 
  {
    fprintf(stderr, "%% Failed to produce to topic %s\n"
                    "partition %i: %s\n", rd_kafka_topic_name(ns_kaka_conn_ptr->rkt), ns_kaka_conn_ptr->partition,
                     rd_kafka_err2str(rd_kafka_last_error()));

    /* Poll to handle delivery reports */
    rd_kafka_poll(ns_kaka_conn_ptr->rk, 0);

    return NS_KAFKA_ERROR;
  }

  rd_kafka_poll(ns_kaka_conn_ptr->rk, 0);

  int run = 1;
  /* Wait for messages to be delivered */
   while (run && rd_kafka_outq_len(ns_kaka_conn_ptr->rk) > 0)
     rd_kafka_poll(ns_kaka_conn_ptr->rk, 10);

  if(!msg_delivery_flag)
  {
    fprintf(stderr, "kafka_producer_ERROR\n");
    return NS_KAFKA_ERROR;
  }

  return NS_KAFKA_SUCCESS;
}

int ns_kakfa_close_connection(NSKafkaClientConn *ns_kaka_conn_ptr)
{
  #ifdef NS_DEBUG_ON
    fprintf(stderr, "Method Entry. ns_kafka_close_connection()\n");
  #endif

  /* Destroy topic */
  rd_kafka_topic_destroy(ns_kaka_conn_ptr->rkt);

  /* Destroy the handle */
  rd_kafka_destroy(ns_kaka_conn_ptr->rk);

  if(ns_kaka_conn_ptr->topic_conf)
    rd_kafka_topic_conf_destroy(ns_kaka_conn_ptr->topic_conf);

  /* Let background threads clean up and terminate cleanly. */
  int run = 100;
  while(run-- > 0 && rd_kafka_wait_destroyed(1000) == -1)
    fprintf(stderr, "Waiting for librdkafka to decommission\n");

  if(run <= 0)
    rd_kafka_dump(stdout, ns_kaka_conn_ptr->rk);

  return NS_KAFKA_SUCCESS;
}

#if 0
static void print_partition_list (FILE *fp,
                                  const rd_kafka_topic_partition_list_t
                                  *partitions) {
        int i;
        for (i = 0 ; i < partitions->cnt ; i++) {
                fprintf(stderr, "%s %s [%"PRId32"] offset %"PRId64,
                        i > 0 ? ",":"",
                        partitions->elems[i].topic,
                        partitions->elems[i].partition,
                        partitions->elems[i].offset);
        }
        fprintf(stderr, "\n");

}
#endif

static void rebalance_cb (rd_kafka_t *rk, rd_kafka_resp_err_t err, rd_kafka_topic_partition_list_t *partitions, void *opaque) 
{
  static int wait_eof = 0;  /* number of partitions awaiting EOF */

  switch (err)
  {
    case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
            //fprintf(stderr, "assigned:\n");
            //print_partition_list(stderr, partitions);
            rd_kafka_assign(rk, partitions);
            wait_eof += partitions->cnt;
            break;
 
    case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
            //fprintf(stderr, "revoked:\n");
            //print_partition_list(stderr, partitions);
            rd_kafka_assign(rk, NULL);
            wait_eof = 0;
            break;
 
    default:
            //fprintf(stderr, "failed: %s\n", rd_kafka_err2str(err));
            rd_kafka_assign(rk, NULL);
            break;
  }
}

int ns_kafka_make_consumer_connection(NSKafkaClientConn *ns_kaka_conn_ptr, char *server_ip, char *consumer_group_name, char *topic_name)
{
  #ifdef NS_DEBUG_ON
    fprintf(stderr, "Method Entry. ns_make_kafka_connection()\n");
  #endif

  char errstr[512];
  char tmp[16];
  int err;

  ns_kaka_conn_ptr->conf = rd_kafka_conf_new();

  /* Set logger */
  //rd_kafka_conf_set_log_cb(ns_kaka_conn_ptr->conf, logger);

  snprintf(tmp, sizeof(tmp), "%i", SIGIO);
  rd_kafka_conf_set(ns_kaka_conn_ptr->conf, "internal.termination.signal", tmp, NULL, 0);

  ns_kaka_conn_ptr->topic_conf = rd_kafka_topic_conf_new();

  /* Consumer groups require a group id */
  //char *group = "test-consumer-group";
  char *group = "rdkafka_consumer_example";

  if(consumer_group_name)
    group = consumer_group_name;

  if (rd_kafka_conf_set(ns_kaka_conn_ptr->conf, "group.id", group, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) 
  {
    fprintf(stderr, "%% %s\n", errstr);
    return NS_KAFKA_ERROR;
  }

  /* Consumer groups always use broker based offset storage */
  if (rd_kafka_topic_conf_set(ns_kaka_conn_ptr->topic_conf, "offset.store.method", "broker", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) 
  {
    fprintf(stderr, "%% %s\n", errstr);
    return NS_KAFKA_ERROR;
  }

  /* Set default topic config for pattern-matched topics. */
  rd_kafka_conf_set_default_topic_conf(ns_kaka_conn_ptr->conf, ns_kaka_conn_ptr->topic_conf);

  /* Callback called on partition assignment changes */
  //rd_kafka_conf_set_rebalance_cb(ns_kaka_conn_ptr->conf, rebalance_cb);

  if (!(ns_kaka_conn_ptr->rk = rd_kafka_new(RD_KAFKA_CONSUMER, ns_kaka_conn_ptr->conf, errstr, sizeof(errstr))))
  {
    fprintf(stderr, "Error: Failed to create new producer: %s\n", errstr);
    return NS_KAFKA_ERROR;
  }

  /* Add brokers */
  if (rd_kafka_brokers_add(ns_kaka_conn_ptr->rk, server_ip) == 0)
  {
    fprintf(stderr, "Error: No valid brokers specified\n");
    return NS_KAFKA_ERROR;
  }

  rd_kafka_poll_set_consumer(ns_kaka_conn_ptr->rk);
  rd_kafka_topic_partition_list_t *topics = rd_kafka_topic_partition_list_new(1);

  int32_t partition = -1;
  rd_kafka_topic_partition_list_add(topics, topic_name, partition);

  #ifdef NS_DEBUG_ON
    fprintf(stderr, "%% Subscribing to %d topics\n", topics->cnt);
  #endif

  if ((err = rd_kafka_subscribe(ns_kaka_conn_ptr->rk, topics))) 
  {
    fprintf(stderr, "%% Failed to start consuming topics: %s\n", rd_kafka_err2str(err));
    return NS_KAFKA_ERROR;
  } 

  return NS_KAFKA_SUCCESS;
}

int ns_kafka_consume_msg(NSKafkaClientConn *ns_kaka_conn_ptr, char *topic_name)
{
  #ifdef NS_DEBUG_ON
    fprintf(stderr, "Method Entry. ns_kafka_consume_msg()\n");
  #endif

  rd_kafka_message_t *rkmessage;

  int run = 1;

  while(run)
  {
    rkmessage = rd_kafka_consumer_poll(ns_kaka_conn_ptr->rk, 1000);
    if (rkmessage) 
    {
      if (rkmessage->err) 
      {
        if (rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) 
        {
          fprintf(stderr, "%% Consumer reached end of %s [%"PRId32"]" "message queue at offset %"PRId64"\n",
                           rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition, rkmessage->offset);
          rd_kafka_message_destroy(rkmessage);
          return NS_KAFKA_ERROR;
        }
  
        if (rkmessage->rkt)
        {
          fprintf(stderr, "%% Consume error for topic \"%s\" [%"PRId32"] " "offset %"PRId64": %s\n",
                           rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition,
                           rkmessage->offset, rd_kafka_message_errstr(rkmessage));
          rd_kafka_message_destroy(rkmessage);
          return NS_KAFKA_ERROR;
        }
        else
        {
          fprintf(stderr, "%% Consumer error: %s: %s\n",
                           rd_kafka_err2str(rkmessage->err), rd_kafka_message_errstr(rkmessage));
          rd_kafka_message_destroy(rkmessage);
          return NS_KAFKA_ERROR;
        }
 
        if (rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION || rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC)
        {
          fprintf(stderr, "UNKNOWN_PARTITION or UNKNOWN_TOPIC\n");
          rd_kafka_message_destroy(rkmessage);
          return NS_KAFKA_ERROR;
        }
      }
      else
      {
        #ifdef NS_DEBUG_ON 
          fprintf(stderr, "Message = %.*s\n", (int)rkmessage->len, (char *)rkmessage->payload); 
        #endif
        run = 0; 
      }
      rd_kafka_message_destroy(rkmessage);
    }
    else
    {
      fprintf(stderr, "There is no Messgae on %s\n", topic_name);
      return NS_KAFKA_ERROR;
    }
  }
}
