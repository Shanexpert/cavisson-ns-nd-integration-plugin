#ifndef _NS_SMTP_SEND_H_
#define _NS_SMTP_SEND_H_

extern unsigned char *base64_encode_vector();
extern void smtp_send_ehlo(connection *cptr, u_ns_ts_t now);
extern void smtp_send_helo(connection *cptr, u_ns_ts_t now) ;
extern void smtp_send_auth_login(connection *cptr, u_ns_ts_t now) ;
extern void smtp_send_auth_login_user_id(connection *cptr, u_ns_ts_t now);
extern void smtp_send_auth_login_passwd(connection *cptr, u_ns_ts_t now);
extern void smtp_send_mail(connection *cptr, u_ns_ts_t now);
extern void smtp_send_quit(connection *cptr, u_ns_ts_t now) ;
extern void smtp_send_rcpt(connection *cptr, u_ns_ts_t now);
extern void smtp_send_rcpt_cc(connection *cptr, u_ns_ts_t now);
extern void smtp_send_rcpt_bcc(connection *cptr, u_ns_ts_t now);
extern void smtp_send_data(connection *cptr, u_ns_ts_t now);
extern void smtp_send_data_body(connection *cptr, u_ns_ts_t now);
extern void smtp_timeout_handle( ClientData client_data, u_ns_ts_t now );
extern void add_smtp_timeout_timer(connection *cptr, u_ns_ts_t now);
extern void smtp_send_starttls(connection *cptr, u_ns_ts_t now);
extern void handle_smtp_ssl_write(connection *cptr, u_ns_ts_t now);
#endif  /* _NS_SMTP_SEND_H_  */
