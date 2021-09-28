#ifndef __NS_POP3_H__
#define __NS_POP3_H__


/* POP3 action types STAT/LIST/GET */
#define POP3_ACTION_STAT        1
#define POP3_ACTION_LIST        2
#define POP3_ACTION_GET         3


/* pop3 protocol states */
/* Any change in protocol states should also be reflected in g_pop3_st_str[][] */
/* Should always start with 0 since we are filling 0 in cptr init */
#define ST_POP3_INITIALIZATION  0
#define ST_POP3_CONNECTED       1
#define ST_POP3_USER            2
#define ST_POP3_PASS            3
#define ST_POP3_STAT            4
#define ST_POP3_LIST            5
#define ST_POP3_RETR            6
#define ST_POP3_DELE            7
#define ST_POP3_QUIT            8
#define ST_POP3_STLS_LOGIN      9
#define ST_POP3_STLS            10


/* pop3 cptr header states */
#define POP3_HDST_NEW           0
#define POP3_HDST_PLUS          1
#define POP3_HDST_PLUS_O        2
#define POP3_HDST_MINUS         3
#define POP3_HDST_MINUS_E       4
#define POP3_HDST_MINUS_ER      5
#define POP3_HDST_TEXT          6
#define POP3_HDST_TEXT_CR       7
#define POP3_HDST_TEXT_CRLF     8
#define POP3_HDST_TEXT_CRLF_DOT 9
#define POP3_HDST_TEXT_CRLF_DOT_CR      10
#define POP3_HDST_END           11

/* POP3 response code */
#define POP3_OK   1
#define POP3_ERR  0


/* POP3 cmds */
#define POP3_CMD_USER   "USER "
#define POP3_CMD_PASS   "PASS "
#define POP3_CMD_STAT   "STAT\r\n"
#define POP3_CMD_LIST   "LIST\r\n"
#define POP3_CMD_RETR   "RETR "
#define POP3_CMD_DELE   "DELE "
#define POP3_CMD_RSET   "RSET\r\n"
#define POP3_CMD_QUIT   "QUIT\r\n"
#define POP3_CMD_STLS   "STLS\r\n"
#define POP3_CMD_CRLF   "\r\n"


#define RESET_URL_RESP_AND_CPTR_VPTR_BYTES {            \
    cptr->bytes = vptr->bytes = 0;			\
    url_resp_buff[0] = 0;				\
  }

/* Function prototypes */
extern void pop3_timeout_handle( ClientData client_data, u_ns_ts_t now );
extern void delete_pop3_timeout_timer(connection *cptr);
extern int handle_pop3_read( connection *cptr, u_ns_ts_t now );
extern char *pop3_state_to_str(int state);

extern void pop3_save_scan_listing(connection *cptr, char *buf, int bytes_read);
extern int pop3_msg_left_in_scan_listing(connection *cptr);
extern char *pop3_get_next_from_scan_listing(connection *cptr);
extern char *pop3_get_last_fetched_from_scan_listing(connection *cptr);
extern void pop3_free_scan_listing(connection *cptr);
extern void debug_log_pop3_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);
extern void pop3_send_stls(connection *cptr, u_ns_ts_t now);
extern void pop3_send_user(connection *cptr, u_ns_ts_t now);
#endif  /* __NS_POP3_H__ */
