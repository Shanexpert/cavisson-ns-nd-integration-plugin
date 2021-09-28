

#define INVALID_TYPE -1
#define NO_DATA_TO_WRITE -2
#define NULL_DESTINATION -3
#define SUCCESS 1
#define ERROR 0

#define RPR_DL(cptr, ...) rpr_debug_log_ex(_FLN_, cptr, __VA_ARGS__)
#define PARENT (parent_pid == getpid())
#define RPR_EXIT(exit_status, ...) rpr_exit(_FLN_, exit_status, __VA_ARGS__)
#define rpr_error_log rpr_error_log_ex
#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__

#define MY_REALLOC(buf, size, cptr, msg) {                              \
    if (size <= 0) {                                                    \
      rpr_error_log(_FLN_, cptr, "Trying to malloc a negative or 0 size (%d)", size); \
      exit(1);                                                          \
    } else {                                                            \
      buf = (void*)realloc(buf, size);                                  \
      if ( buf == (void*) 0 ) {                                         \
        rpr_error_log(_FLN_, cptr, "Out of Memmory: %s", msg); \
        exit(1);                                                        \
      }                                                                 \
      RPR_DL(cptr, "MY_REALLOC'ed (%s) done. ptr = %p, size = %d", msg, buf, size); \
    }                                                                   \
  }

#define DEFAULT_CA_LIST          "cav-test-root.pem"
#define DEFAULT_DH_FILE          "dh1024.pem"
#define DEFAULT_RANDOM_FILE      "random.pem"

#define SSL2_3        0
#define SSL3_0        1
#define TLS1_0        2
#define TLS1_1        3
#define TLS1_2        4
#define TLS1_3        5

#define MAX_FILE 100
#define MAX_FILE_LEN 1024
#define MAX_FILE_NAME_LEN 2048


#define DEFAULT_MAX_DEBUG_FILE_SIZE 100000000
#define DEFAULT_MAX_ERROR_FILE_SIZE 10000000
#define MAX_LOG_BUF_SIZE  64000
#define ERROR_BUF_SIZE 4096

#define DEBUG_HEADER "\nAbsolute Time Stamp|File|Line|Function|Child Index|Parent/Child ID|Request ID|FD|State|Log Messages"

extern int debug_log;
extern int g_rpr_id;
extern char rpr_conf_dir[MAX_FILE_NAME_LEN];
extern char g_rpr_logs_dir[MAX_FILE_NAME_LEN];
extern int debug_log;
extern int error_fd;
extern int debug_fd;


extern int get_tokens(char *read_buf, char *fields[], char *delim, int max_flds);
extern void init_ssl();
extern void init_ssl_default();
extern void kw_set_ssl_cert_chain_file(char *keyword, char *text);
extern void kw_set_ssl_cacert_file(char *keyword, char *text);
extern void kw_set_ssl_revocation_file(char *keyword, char *text);
extern void kw_set_ssl_verify_depth(char *keyword, char *text);
extern int hpd_berr_exit(char *string);
extern SSL *ssl_set_client(int rfd);
extern void kw_set_ssl_extra_cert_chain_file(char *keyword, char *text);
extern void kw_set_ssl_cert_pass(char *buff);
extern SSL_CTX *initialize_ctx_client(char *keyfile);
extern void rpr_berr_exit(char *string);
extern void rpr_debug_log_ex(char *file, int line, char *fname, void *cptr_void, char *format, ...);
extern void rpr_exit(char *file, int line, char *fname, int exit_status, char *format, ...);
extern char *rpr_get_cur_date_time();
extern void open_log(char *name, int *fd, unsigned int max_size, char *header);
extern void rpr_error_log_ex(char *file, int line, char *fname, void *cptr_void, char *format, ...);
extern void rpr_write_all(int fd, char *buf, int size);
extern void set_ssl(connection *cptr, u_ns_ts_t now);
extern u_ns_ts_t get_ms_stamp();
extern timer_type* dis_timer_next(u_ns_ts_t now);
extern void dis_timer_think_add(int type, timer_type* tmr, u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data, int periodic);
extern inline void dis_timer_del(timer_type* tmr);
extern inline void dis_timer_add(int type, timer_type* tmr, u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data, int periodic);
extern void dis_timer_run(u_ns_ts_t now);
extern inline void dis_idle_timer_reset (u_ns_ts_t now, timer_type* tmr);
extern void *ssl_set_clients_local(connection *cptr, int rfd, char *host_name, char *client_cert);
extern void handle_ssl_accept(connection *cptr, u_ns_ts_t now);
