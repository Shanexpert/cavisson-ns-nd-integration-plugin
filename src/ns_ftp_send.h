#ifndef __NS_FTP_SEND_H__
#define  __NS_FTP_SEND_H__

extern void ftp_timeout_handle( ClientData client_data, u_ns_ts_t now );
extern void add_ftp_timeout_timer(connection *cptr, u_ns_ts_t now);
extern void debug_log_ftp_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);
extern void ftp_send_user(connection *cptr, u_ns_ts_t now);
extern void ftp_send_pass(connection *cptr, u_ns_ts_t now);
extern void ftp_send_pasv(connection *cptr, u_ns_ts_t now);
extern void ftp_send_port(connection *cptr, u_ns_ts_t now);
extern void ftp_send_retr(connection *cptr, u_ns_ts_t now);
extern void ftp_send_quit(connection *cptr, u_ns_ts_t now);

#endif  /*   __NS_FTP_SEND_H__ */
