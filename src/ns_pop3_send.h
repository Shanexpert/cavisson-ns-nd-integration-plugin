#ifndef __NS_POP3_SEND_H__
#define __NS_POP3_SEND_H__

extern void pop3_send_user(connection *cptr, u_ns_ts_t now);
extern void pop3_send_pass(connection *cptr, u_ns_ts_t now);
extern void pop3_send_stat(connection *cptr, u_ns_ts_t now);
extern void pop3_send_list(connection *cptr, u_ns_ts_t now);
extern void pop3_send_retr(connection *cptr, u_ns_ts_t now);
extern void pop3_send_dele(connection *cptr, u_ns_ts_t now);
extern void pop3_send_quit(connection *cptr, u_ns_ts_t now);
extern void add_pop3_timeout_timer(connection *cptr, u_ns_ts_t now);

#endif  /* __NS_POP3_SEND_H__ */
