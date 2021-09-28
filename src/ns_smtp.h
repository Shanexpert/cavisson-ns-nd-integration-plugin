#ifndef _NS_SMTP_H_
#define _NS_SMTP_H_

#define SMTP_HDST_RCODE_X               0
#define SMTP_HDST_RCODE_Y               1
#define SMTP_HDST_RCODE_Z               2
#define SMTP_HDST_SPACE_HYPHEN          3
#define SMTP_HDST_SPACE_TEXT            4
#define SMTP_HDST_SPACE_CFLR            5
#define SMTP_HDST_HYPHEN_TEXT           6
#define SMTP_HDST_HYPHEN_CFLR           7
#define SMTP_HDST_END                   8
#define SMTP_HDST_HYPHEN_TEXT_S         9
#define SMTP_HDST_HYPHEN_TEXT_ST        10
#define SMTP_HDST_HYPHEN_TEXT_STA       11
#define SMTP_HDST_HYPHEN_TEXT_STAR      12
#define SMTP_HDST_HYPHEN_TEXT_START     13
#define SMTP_HDST_HYPHEN_TEXT_STARTT    14
#define SMTP_HDST_HYPHEN_TEXT_STARTTL   15
#define SMTP_HDST_HYPHEN_TEXT_STARTTLS  16

/* Cmds */
#define SMTP_CMD_EHLO           "EHLO cavisson.com\r\n"
#define SMTP_CMD_HELO           "HELO cavisson.com\r\n"
#define SMTP_CMD_STARTTLS       "STARTTLS\r\n"
#define SMTP_CMD_AUTH_LOGIN     "AUTH LOGIN\r\n"
#define SMTP_CMD_DATA           "DATA\r\n"
#define SMTP_CMD_QUIT           "QUIT\r\n"
#define SMTP_CMD_BODY_TERM      "\r\n.\r\n"
#define SMTP_CMD_CRLF           "\r\n"

#define SMTP_CMD_CRLFCRLF       "\r\n\r\n"

/* protocol states */
/* Any change in protocol states should also be reflected in g_smtp_st_str[][] */
/* Should always start with 0 since we are filling 0 in cptr init */
#define ST_SMTP_INITIALIZATION  0
#define ST_SMTP_CONNECTED       1
#define ST_SMTP_HELO            2
#define ST_SMTP_EHLO            3
#define ST_SMTP_AUTH_LOGIN      4
#define ST_SMTP_AUTH_LOGIN_USER_ID      5
#define ST_SMTP_AUTH_LOGIN_PASSWD       6
#define ST_SMTP_MAIL            7
#define ST_SMTP_RCPT            8
#define ST_SMTP_RCPT_CC         9
#define ST_SMTP_RCPT_BCC        10
#define ST_SMTP_DATA            11
#define ST_SMTP_DATA_BODY       12
#define ST_SMTP_RSET            13
#define ST_SMTP_QUIT            14
#define ST_SMTP_STARTTLS_LOGIN  15
#define ST_SMTP_STARTTLS        16

extern char g_smtp_st_str[][0xff];

extern int handle_smtp_read( connection *cptr, u_ns_ts_t now );
extern void encode( FILE *infile, FILE *outfile, int linesize );

extern void delete_smtp_timeout_timer(connection *cptr);
extern char *smtp_state_to_str(int state);
extern void debug_log_smtp_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);

#endif  /* _NS_SMTP_H_  */
