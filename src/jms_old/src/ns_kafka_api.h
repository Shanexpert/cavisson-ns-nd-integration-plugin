#ifndef NS_KAFKA_API_H
#define NS_KAFKA_API_H

#include <rdkafka.h>

#define NS_KAFKA_ERROR   -1
#define NS_KAFKA_SUCCESS  0

typedef struct
{
  rd_kafka_topic_t        *rkt;
  rd_kafka_conf_t         *conf;
  rd_kafka_topic_conf_t   *topic_conf;
  rd_kafka_t              *rk;
  int                     partition;

}NSKafkaClientConn;

extern int ns_make_kafka_producer_connection(NSKafkaClientConn *ns_kaka_conn_ptr, char *server_ip, char *topic_name);
extern int ns_kafka_make_consumer_connection(NSKafkaClientConn *ns_kaka_conn_ptr, char *server_ip, char *consumer_group_name, char *queue_or_topic_name);
extern int ns_kafka_produce_msg(NSKafkaClientConn *ns_kaka_conn_ptr, char *msg);
extern int ns_kafka_consume_msg(NSKafkaClientConn *ns_kaka_conn_ptr, char *queue_or_topic_name);
extern int ns_kakfa_close_connection(NSKafkaClientConn *ns_kaka_conn_ptr);

#endif
