
#ifndef __ns_xmpp
#define __ns_xmpp

#define XMPP_MAX_ATTR_LEN       1024

#define XMPP_BUF_SIZE_XS  128
#define XMPP_BUF_SIZE_S   512 
#define XMPP_BUF_SIZE_M   1024
#define XMPP_BUF_SIZE_L   4096
#define XMPP_BUF_SIZE_XL  10240
#define XMPP_BUF_SIZE_XXL 65535
#define XMPP_SEND_BUF_SIZE XMPP_BUF_SIZE_XL + XMPP_BUF_SIZE_S
#define MAX_STATE_MODEL_TOKEN 20

#define CNONCE_LEN 8

#define START_STREAM 0
#define FEATURE_STREAM 1
#define STARTTLS_STREAM 2
#define SASL_STREAM 3
#define MESSAGE_STREAM 4


#define STARTTLS_DISSABLE 0
#define STARTTLS_ENABLE   1

#define NOT_ACCEPT_CONTACT 0
#define ACCEPT_CONTACT     1

#define NS_XMPP_USER 0
#define NS_XMPP_GROUP 1

#define NS_XMPP_LOGIN		0 
#define NS_XMPP_SEND_MESSAGE	1 
#define NS_XMPP_ADD_CONTACT	2
#define NS_XMPP_DELETE_CONTACT	3
#define NS_XMPP_CREATE_GROUP	4
#define NS_XMPP_DELETE_GROUP	5
#define NS_XMPP_JOIN_GROUP	6
#define NS_XMPP_LEAVE_GROUP	7
#define NS_XMPP_ADD_MEMBER	8
#define NS_XMPP_DELETE_MEMBER	9


#define CLIENT_KEY "Client Key"

#define XMPP_DATA_INTO_AVG(src) {\
  xmpp_avgtime->src++;\
  if(SHOW_GRP_DATA) { \
    XMPPAvgTime *local_xmpp_avg =  NULL; \
    local_xmpp_avg = (XMPPAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_xmpp_avgtime_idx); \
    local_xmpp_avg->src++; \
   } \
}

#define XMPP_DATA_INTO_AVG_THROUGHPUT(dest, src) {\
  xmpp_avgtime->dest += src;\
  if(SHOW_GRP_DATA) { \
    XMPPAvgTime *local_xmpp_avg = NULL; \
    local_xmpp_avg = (XMPPAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_xmpp_avgtime_idx); \
    local_xmpp_avg->dest += src; \
   } \
}

#define ACC_XMPP_CUMULATIVES(a, b)\
  (a)->xmpp_c_login_attempted += (b)->xmpp_login_attempted;\
  (a)->xmpp_c_login_succ += (b)->xmpp_login_succ;\
  (a)->xmpp_c_login_failed += (b)->xmpp_login_failed;\
  (a)->xmpp_c_msg_sent += (b)->xmpp_msg_sent;\
  (a)->xmpp_c_msg_send_failed += (b)->xmpp_msg_send_failed;\
  (a)->xmpp_c_msg_rcvd += (b)->xmpp_msg_rcvd;\
  (a)->xmpp_c_msg_dlvrd += (b)->xmpp_msg_dlvrd;\
  (a)->xmpp_c_send_bytes += (b)->xmpp_send_bytes;\
  (a)->xmpp_c_rcvd_bytes += (b)->xmpp_rcvd_bytes;\
  (a)->xmpp_c_login_completed += (b)->xmpp_login_completed;\

#define ACC_XMPP_CUMULATIVES_PARENT(a, b)\
  (a)->xmpp_c_login_attempted += (b)->xmpp_c_login_attempted;\
  (a)->xmpp_c_login_succ += (b)->xmpp_c_login_succ;\
  (a)->xmpp_c_login_failed += (b)->xmpp_c_login_failed;\
  (a)->xmpp_c_msg_sent += (b)->xmpp_c_msg_sent;\
  (a)->xmpp_c_msg_send_failed += (b)->xmpp_c_msg_send_failed;\
  (a)->xmpp_c_msg_rcvd += (b)->xmpp_c_msg_rcvd;\
  (a)->xmpp_c_msg_dlvrd += (b)->xmpp_c_msg_dlvrd;\
  (a)->xmpp_c_send_bytes += (b)->xmpp_c_send_bytes;\
  (a)->xmpp_c_rcvd_bytes += (b)->xmpp_c_rcvd_bytes; \
  (a)->xmpp_c_login_completed += (b)->xmpp_c_login_completed;\

#define ACC_XMPP_PERIODICS(a, b)\
  (a)->xmpp_login_attempted += (b)->xmpp_login_attempted;\
  (a)->xmpp_login_succ += (b)->xmpp_login_succ;\
  (a)->xmpp_login_failed += (b)->xmpp_login_failed;\
  (a)->xmpp_msg_sent += (b)->xmpp_msg_sent;\
  (a)->xmpp_msg_send_failed += (b)->xmpp_msg_send_failed;\
  (a)->xmpp_msg_rcvd += (b)->xmpp_msg_rcvd;\
  (a)->xmpp_msg_dlvrd += (b)->xmpp_msg_dlvrd;\
  (a)->xmpp_send_bytes += (b)->xmpp_send_bytes;\
  (a)->xmpp_rcvd_bytes += (b)->xmpp_rcvd_bytes;\
  (a)->xmpp_login_completed += (b)->xmpp_login_completed;\

#define CHILD_RESET_XMPP_STAT_AVGTIME(a) \
{ \
  XMPPAvgTime *loc_xmpp_avgtime; \
  loc_xmpp_avgtime = (XMPPAvgTime*)((char*)a + g_xmpp_avgtime_idx); \
  loc_xmpp_avgtime->xmpp_login_attempted = 0;\
  loc_xmpp_avgtime->xmpp_login_succ = 0;\
  loc_xmpp_avgtime->xmpp_login_failed = 0; \
  loc_xmpp_avgtime->xmpp_msg_sent = 0; \
  loc_xmpp_avgtime->xmpp_msg_send_failed = 0; \
  loc_xmpp_avgtime->xmpp_msg_rcvd = 0; \
  loc_xmpp_avgtime->xmpp_msg_dlvrd = 0; \
  loc_xmpp_avgtime->xmpp_send_bytes = 0; \
  loc_xmpp_avgtime->xmpp_rcvd_bytes = 0; \
  loc_xmpp_avgtime->xmpp_login_completed = 0;\
}

typedef struct XMPPAvgTime{
  u_ns_4B_t xmpp_login_attempted;
  u_ns_4B_t xmpp_login_succ;
  u_ns_4B_t xmpp_login_failed;
  u_ns_4B_t xmpp_msg_sent;
  u_ns_4B_t xmpp_msg_send_failed;
  u_ns_4B_t xmpp_msg_rcvd;
  u_ns_4B_t xmpp_msg_dlvrd;
  u_ns_4B_t xmpp_send_bytes;
  u_ns_4B_t xmpp_rcvd_bytes;
  u_ns_4B_t xmpp_login_completed;
}XMPPAvgTime;

typedef struct XMPPCAvgTime{
  u_ns_4B_t xmpp_c_login_attempted;
  u_ns_4B_t xmpp_c_login_succ;
  u_ns_4B_t xmpp_c_login_failed;
  u_ns_4B_t xmpp_c_msg_sent;
  u_ns_4B_t xmpp_c_msg_send_failed;
  u_ns_4B_t xmpp_c_msg_rcvd;
  u_ns_4B_t xmpp_c_msg_dlvrd;
  u_ns_4B_t xmpp_c_send_bytes;
  u_ns_4B_t xmpp_c_rcvd_bytes;
  u_ns_4B_t xmpp_c_login_completed;
}XMPPCAvgTime;

extern XMPPCAvgTime *xmpp_cavgtime;

#ifndef CAV_MAIN
extern int g_xmpp_avgtime_idx;
extern XMPPAvgTime *xmpp_avgtime;
#else
extern __thread int g_xmpp_avgtime_idx;
extern __thread XMPPAvgTime *xmpp_avgtime;
#endif
extern int g_xmpp_cavgtime_idx;

extern int xmpp_start_stream(connection *cptr, u_ns_ts_t now);
extern int xmpp_close_disconnect(connection *cptr, u_ns_ts_t now, int close_stream, int close_connection);
extern int nsi_xmpp_send(VUser *vptr, u_ns_ts_t now);
extern int nsi_xmpp_logout(VUser *vptr, u_ns_ts_t now);
extern int do_xmpp_complete(VUser *vptr);
extern int xmpp_tls_proceed(connection *cptr, u_ns_ts_t now);
extern int xmpp_sasl_auth(connection *cptr, u_ns_ts_t now);
extern int xmpp_sasl_resp(connection *cptr, u_ns_ts_t now);
extern int xmpp_do_bind(connection *cptr, u_ns_ts_t now);
extern int xmpp_process_message(connection *cptr, u_ns_ts_t now);
extern int xmpp_send_group_presence(connection *cptr, u_ns_ts_t now);
extern int xmpp_create_group(connection *cptr, u_ns_ts_t now);
extern int xmpp_start_tls(connection *cptr, u_ns_ts_t now);
extern int xmpp_resource_bind(connection *cptr);
extern int xmpp_start_session(connection *cptr, u_ns_ts_t now);
extern int xmpp_send_presence(connection *cptr, u_ns_ts_t now);
extern int xmpp_accept_contact(connection *cptr, u_ns_ts_t now);
extern int xmpp_send_subscribe(connection *cptr, u_ns_ts_t now);
extern int xmpp_send_result(connection *cptr, u_ns_ts_t now);
extern int xmpp_send_subscribed(connection *cptr, u_ns_ts_t now);
extern int xmpp_send_subscription(connection *cptr, u_ns_ts_t now);
extern int xmpp_send_muc_owner(connection *cptr, u_ns_ts_t now);
extern int xmpp_send_muc_config(connection *cptr, u_ns_ts_t now);
extern int xmpp_read(connection *cptr, u_ns_ts_t now);
extern int xmpp_join_group(connection *cptr, u_ns_ts_t now);
extern int xmpp_delete_group(connection *cptr, u_ns_ts_t now);
extern int xmpp_http_file_upload(connection *cptr, u_ns_ts_t now);
extern int url_hash_get_url_idx_for_dynamic_urls(u_ns_char_t *url, int iUrlLen, int page_id, int static_dynamic, int static_url_id, char *page_name);
extern int xmpp_update_login_stats(VUser *vptr);
extern inline void update_xmpp_avgtime_size();
extern inline void update_xmpp_cavgtime_size();
extern inline void fill_xmpp_gp (avgtime **g_avg, cavgtime **g_cavg);
extern void set_xmpp_avgtime_ptr();
#endif
